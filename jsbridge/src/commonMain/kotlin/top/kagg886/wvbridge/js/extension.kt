package top.kagg886.wvbridge.js

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.withTimeout
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.WebViewBridgeExtInstallScript
import top.kagg886.wvbridge.js.internal.base64Encode
import top.kagg886.wvbridge.js.internal.iife
import top.kagg886.wvbridge.js.protocol.JSPacket
import top.kagg886.wvbridge.js.protocol.JSValue
import kotlin.time.Duration
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid


/**
 * Evaluates [script] as a JavaScript function body and normalizes the result to [JSValue].
 *
 * The wrapper sends [script] to the WebView as UTF-8 Base64, decodes it in the page, and executes
 * it with `Function(script).apply(globalThis)`. Because [script] is compiled as a function body,
 * callers must use `return` to produce a value.
 */
public suspend fun JavaScriptBridge.evaluateScriptValue(script: String): JSValue {
    ensureJavaScriptBridgePostMessageInstalled()

    val script = iife(
        script = """
            const bridge = window.__wvbridge__;

            try {
                const script = bridge.decodeBase64("${script.base64Encode()}");
                return bridge.wrapWire(
                    bridge.valueHeader,
                    bridge.toJSValueObject(Function(script).apply(globalThis))
                );
            } catch (error) {
                return bridge.wrapWire(
                    bridge.valueHeader,
                    bridge.toErrorValueObject(error)
                );
            }
        """.trimIndent()
    )

    return with(JSValue) {
        evaluateScript(script).toJavaScriptBridgeValue()
    }
}

/**
 * Registers a JavaScript bridge message handler for packets whose packet type matches [type].
 *
 * On first use this installs the bridge post-message bootstrap script into the document-start hook
 * list and evaluates it in the current page, then delegates to [JavaScriptBridge.registerWebMessageHandler]
 * to receive raw WebView messages. Incoming messages that cannot be decoded as [JSPacket] or whose
 * packet type differs from [type] are ignored.
 *
 * The returned [CloseHandle] unregisters the underlying raw WebView message handler.
 *
 * @param type application-level packet type to accept.
 * @param handle callback invoked with the decoded packet payload.
 */
public suspend fun JavaScriptBridge.registerWebMessageHandler(type: String, handle: (JSValue) -> Unit): CloseHandle {
    ensureJavaScriptBridgePostMessageInstalled()

    return registerWebMessageHandler { message ->
        val packet = runCatching {
            with(JSPacket) {
                message.toJSPacket()
            }
        }.getOrNull() ?: return@registerWebMessageHandler

        if (packet.type != type) {
            return@registerWebMessageHandler
        }

        handle(packet.message)
    }
}


/**
 * Dispatches a typed message from native code to JavaScript listeners registered with
 * `window.wvbridge.addEventListener(type, listener)`.
 *
 * On first use this installs the bridge post-message bootstrap script into the document-start hook
 * list and evaluates it in the current page. [value] is then delivered to listeners as
 * `listener(value)` by calling `window.wvbridge.dispatchEvent(type, value)`.
 *
 * Only [JSValue.Undefined], [JSValue.Null], and [JSValue.Serializable] can be sent because the
 * page-side value must be representable as `undefined`, `null`, or JSON.
 *
 * @param type application-level packet type to dispatch.
 * @param value payload delivered to matching JavaScript listeners.
 */
public suspend fun JavaScriptBridge.postMessage(type: String, value: JSValue) {
    require(value is JSValue.Undefined || value is JSValue.Null || value is JSValue.Serializable) {
        "JavaScriptBridge.postMessage only supports JSValue.Undefined, JSValue.Null, and JSValue.Serializable"
    }

    ensureJavaScriptBridgePostMessageInstalled()

    val valueExpression = value.toJavaScriptExpression("JavaScriptBridge.postMessage")

    val script = iife(
        script = """
            return window.wvbridge.dispatchEvent(
                window.__wvbridge__.decodeBase64("${type.base64Encode()}"),
                $valueExpression
            );
        """.trimIndent()
    )

    val dispatched = evaluateScript(script)
        .requireJavaScriptBooleanResult("JavaScriptBridge.postMessage")
    check(dispatched) {
        "JavaScriptBridge.postMessage failed: no JavaScript listener handled type '$type'"
    }
}


/**
 * Dispatches a typed message to JavaScript and suspends until JavaScript returns one result.
 *
 * This is the request/response form of [postMessage]. The function installs a temporary native
 * response handler, dispatches [type] through `window.wvbridge.dispatchEvent`, and appends a
 * JavaScript callback after [args]. The page must register its listener with
 * `window.wvbridge.addEventListener(type, listener)` before native code calls this API; otherwise
 * the dispatch has no listener to invoke and the call will wait until [timeout]. The JavaScript
 * listener receives `...args, reply` and should call `reply(result)` once.
 *
 * The returned value is the callback argument normalized as [JSValue]. Only [JSValue.Undefined],
 * [JSValue.Null], and [JSValue.Serializable] can be used as outgoing [args] because they must be
 * representable as `undefined`, `null`, or a [kotlinx.serialization.json.JsonElement] in the page.
 *
 * The temporary response handler is always unregistered before this function returns or throws.
 * If JavaScript does not call the callback before [timeout], this function throws
 * [kotlinx.coroutines.TimeoutCancellationException].
 *
 * @param type application-level packet type to dispatch.
 * @param timeout maximum time to wait for the JavaScript callback.
 * @param args payload values delivered to the JavaScript listener before the response callback.
 * @return the value passed by JavaScript to the response callback.
 */
@OptIn(ExperimentalUuidApi::class)
public suspend fun JavaScriptBridge.postMessageAndReceiveResult(
    type: String,
    timeout: Duration,
    vararg args: JSValue,
): JSValue = coroutineScope {
    ensureJavaScriptBridgePostMessageInstalled()

    val deferred = CompletableDeferred<JSValue>()
    val responseToken = "__wvbridge_result__:${Uuid.random().toHexString()}"
    val handle = registerWebMessageHandler(responseToken) {
        deferred.complete(it)
    }

    try {
        val arguments = args.map {
            it.toJavaScriptExpression("JavaScriptBridge.postMessageAndReceiveResult")
        }

        val script = iife(
            script = """
                const type = window.__wvbridge__.decodeBase64("${type.base64Encode()}");
                const responseType = window.__wvbridge__.decodeBase64("${responseToken.base64Encode()}");
                return window.wvbridge.dispatchEvent(
                    type,
                    ${arguments.joinToString(",\n")},
                    (result) => window.wvbridge.postMessage(responseType, result)
                );
            """.trimIndent()
        )

        val dispatched = evaluateScript(script)
            .requireJavaScriptBooleanResult("JavaScriptBridge.postMessageAndReceiveResult")
        check(dispatched) {
            "JavaScriptBridge.postMessageAndReceiveResult failed: no JavaScript listener handled type '$type'"
        }

        withTimeout(timeout) {
            deferred.await()
        }
    } finally {
        handle.close()
    }
}

private fun JSValue.toJavaScriptExpression(apiName: String): String = when (this) {
    JSValue.Undefined -> "undefined"
    JSValue.Null -> "null"
    is JSValue.Serializable -> value.toString().base64Encode().let {
        "JSON.parse(window.__wvbridge__.decodeBase64(\"$it\"))"
    }

    is JSValue.ScriptObject, is JSValue.Error -> throw IllegalArgumentException(
        "$apiName only supports JSValue.Undefined, JSValue.Null, and JSValue.Serializable"
    )
}

private fun String?.requireJavaScriptBooleanResult(apiName: String): Boolean = when (this) {
    "true", "\"true\"" -> true
    "false", "\"false\"" -> false
    else -> error("$apiName expected JavaScript to return true or false, but got ${this ?: "null"}")
}

private suspend fun JavaScriptBridge.ensureJavaScriptBridgePostMessageInstalled() {
    val installed = evaluateScript("window.__wvbridge__ !== undefined")
        ?.let { it == "true" || it == "\"true\"" }
        ?: false

    if (!installed) {
        registerDocumentStartHook(WebViewBridgeExtInstallScript)
        evaluateScript(WebViewBridgeExtInstallScript)
    }
}

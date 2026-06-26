package top.kagg886.wvbridge.js

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.currentCoroutineContext
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.WebViewBridgeExtInstallScript
import top.kagg886.wvbridge.js.internal.JavaScriptBridgeReplyTokenPrefix
import top.kagg886.wvbridge.js.internal.JavaScriptBridgeResultTokenPrefix
import top.kagg886.wvbridge.js.internal.base64Encode
import top.kagg886.wvbridge.js.internal.iife
import top.kagg886.wvbridge.js.protocol.JSPacket
import top.kagg886.wvbridge.js.protocol.JSValue
import top.kagg886.wvbridge.js.protocol.JavaScriptBridgeMessageHandler
import top.kagg886.wvbridge.js.protocol.JavaScriptBridgeMessageHandlerWithReply
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
 * @param handle callback invoked with the decoded packet payload values.
 */
public suspend fun JavaScriptBridge.registerWebMessageHandler(
    type: String,
    handle: JavaScriptBridgeMessageHandler,
): CloseHandle {
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

        handle.handle(*packet.messages.toTypedArray())
    }
}

/**
 * Registers a JavaScript bridge message handler for packets sent by
 * `window.wvbridge.postMessageAndReceiveResult(type, options)`.
 *
 * The page-side API appends an internal reply token after `options.args`. This function removes
 * that token before invoking [handle] and provides a reply callback that calls the generated
 * `window.__wvbridge__[uuid]` function in the page.
 *
 * Calling [reply] after the JavaScript-side timeout is harmless from Kotlin's perspective, but the
 * page may no longer have the generated function installed.
 *
 * @param type application-level packet type to accept.
 * @param handle callback invoked with decoded payload values and a reply callback.
 */
public suspend fun JavaScriptBridge.registerWebMessageHandlerWithReply(
    type: String,
    handle: JavaScriptBridgeMessageHandlerWithReply,
): CloseHandle {
    ensureJavaScriptBridgePostMessageInstalled()
    val scope = CoroutineScope(SupervisorJob() + currentCoroutineContext().minusKey(Job))
    val rawHandle = registerWebMessageHandler { message ->
        val packet = runCatching {
            with(JSPacket) {
                message.toJSPacket()
            }
        }.getOrNull() ?: return@registerWebMessageHandler

        if (packet.type != type) {
            return@registerWebMessageHandler
        }

        val replyToken = packet.messages.lastOrNull() as? JSValue.ScriptObject
        if (replyToken?.type != "string" || !replyToken.value.startsWith(JavaScriptBridgeReplyTokenPrefix)) {
            return@registerWebMessageHandler
        }

        val replyId = replyToken.value.removePrefix(JavaScriptBridgeReplyTokenPrefix)
        val values = packet.messages.dropLast(1)
        val reply: suspend (JSValue) -> Unit = { value ->
            runCatching {
                val script = iife(
                    script = """
                            const replyId = window.__wvbridge__.decodeBase64("${replyId.base64Encode()}");
                            const reply = window.__wvbridge__[replyId];
                            if (typeof reply !== "function") {
                                return false;
                            }

                            reply(${value.toJavaScriptExpression("JavaScriptBridge.registerWebMessageHandlerWithReply.reply")});
                            return true;
                        """.trimIndent()
                )

                evaluateScript(script)
                    .requireJavaScriptBooleanResult("JavaScriptBridge.registerWebMessageHandlerWithReply.reply")
            }
        }

        scope.launch {
            runCatching {
                handle.handle(values, reply)
            }
        }
    }

    return object : CloseHandle {
        override fun close() {
            scope.cancel()
            rawHandle.close()
        }
    }
}


/**
 * Dispatches a typed message from native code to JavaScript listeners registered with
 * `window.wvbridge.addEventListener(type, listener)`.
 *
 * On first use this installs the bridge post-message bootstrap script into the document-start hook
 * list and evaluates it in the current page. [values] are then delivered to listeners as
 * `listener(...values)` by calling `window.wvbridge.dispatchEvent(type, ...values)`.
 *
 * Only [JSValue.Undefined], [JSValue.Null], and [JSValue.Serializable] can be sent because the
 * page-side value must be representable as `undefined`, `null`, or JSON.
 *
 * @param type application-level packet type to dispatch.
 * @param values payload values delivered to matching JavaScript listeners.
 */
public suspend fun JavaScriptBridge.postMessage(type: String, vararg values: JSValue) {
    ensureJavaScriptBridgePostMessageInstalled()

    val arguments = buildList {
        add("window.__wvbridge__.decodeBase64('${type.base64Encode()}')")

        addAll(
            elements = values.map { value ->
                value.toJavaScriptExpression("JavaScriptBridge.postMessage")
            }
        )
    }

    val script = iife(
        script = """
            return window.wvbridge.dispatchEvent(
                ${arguments.joinToString(",\n")}
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
    val responseToken = "$JavaScriptBridgeResultTokenPrefix${Uuid.random().toHexString()}"
    val handle = registerWebMessageHandler(responseToken) { values ->
        deferred.complete(values.firstOrNull() ?: JSValue.Undefined)
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

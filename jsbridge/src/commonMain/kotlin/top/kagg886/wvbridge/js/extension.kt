package top.kagg886.wvbridge.js

import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.WebViewBridgeExtInstallScript
import top.kagg886.wvbridge.js.internal.base64Encode
import top.kagg886.wvbridge.js.internal.iife
import top.kagg886.wvbridge.js.protocol.JSPacket
import top.kagg886.wvbridge.js.protocol.JSValue


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
 * list and evaluates it in the current page. [value] is then delivered to listeners for [type] by
 * calling `window.wvbridge.dispatchEvent(type, value)`.
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

    val valueExpression = when (value) {
        JSValue.Undefined -> "undefined"
        JSValue.Null -> "null"
        is JSValue.Serializable -> value.value.toString().base64Encode().let {
            "JSON.parse(window.__wvbridge__.decodeBase64(\"$it\"))"
        }

        is JSValue.ScriptObject, is JSValue.Error -> throw IllegalArgumentException("JavaScriptBridge.postMessage only supports JSValue.Undefined, JSValue.Null, and JSValue.Serializable")
    }

    val script = """
        (() => {
            window.wvbridge.dispatchEvent(
                window.__wvbridge__.decodeBase64("${type.base64Encode()}"),
                $valueExpression
            );
        })()
    """.trimIndent()


    evaluateScript(script)
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

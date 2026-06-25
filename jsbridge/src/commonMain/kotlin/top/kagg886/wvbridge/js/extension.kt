package top.kagg886.wvbridge.js

import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.JavaScriptBridgePacketHeader
import top.kagg886.wvbridge.js.internal.JavaScriptBridgePostMessageTemplate
import top.kagg886.wvbridge.js.internal.javaScriptBridgeEvaluateScriptTemplate
import top.kagg886.wvbridge.js.internal.javaScriptBridgePostMessageDispatchTemplate
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
    return with(JSValue) {
        evaluateScript(javaScriptBridgeEvaluateScriptTemplate(script)).toJavaScriptBridgeValue()
    }
}

/**
 * Registers a JavaScript bridge message handler for packets whose packet type matches [type].
 *
 * On first use this installs the bridge post-message bootstrap script into the document-start hook
 * list and evaluates it in the current page, then delegates to [JavaScriptBridge.registerWebMessageHandler]
 * to receive raw WebView messages. Incoming messages that cannot be decoded as [JSPacket], whose header
 * is not the JavaScript bridge header, or whose packet type differs from [type] are ignored.
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

        if (packet.header != JavaScriptBridgePacketHeader || packet.type != type) {
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
    evaluateScript(javaScriptBridgePostMessageDispatchTemplate(type, value))
}

private suspend fun JavaScriptBridge.ensureJavaScriptBridgePostMessageInstalled() {
    if (with(evaluateScriptValue("return window.__wvbridge_jsbridge_initialized__")) { this !is JSValue.ScriptObject || this.type != "boolean" || !this.value.toBooleanStrict() }) {
        registerDocumentStartHook(JavaScriptBridgePostMessageTemplate)
        evaluateScript(JavaScriptBridgePostMessageTemplate)
    }
}

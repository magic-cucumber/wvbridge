package top.kagg886.wvbridge.js

import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.JavaScriptBridgePacketHeader
import top.kagg886.wvbridge.js.internal.JavaScriptBridgePostMessageTemplate
import top.kagg886.wvbridge.js.internal.javaScriptBridgeEvaluateScriptTemplate
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

public suspend fun JavaScriptBridge.registerWebMessageHandler(type: String, handle: (JSValue) -> Unit): CloseHandle {
    if (with(evaluateScriptValue("return window.__wvbridge_jsbridge_initialized__")) { this !is JSValue.ScriptObject || this.type != "boolean" || !this.value.toBooleanStrict() }) {
        registerDocumentStartHook(JavaScriptBridgePostMessageTemplate)
        evaluateScript(JavaScriptBridgePostMessageTemplate)
    }

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

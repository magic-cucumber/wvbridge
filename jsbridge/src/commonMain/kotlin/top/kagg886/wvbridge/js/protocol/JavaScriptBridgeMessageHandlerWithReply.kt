package top.kagg886.wvbridge.js.protocol

/**
 * Handles JavaScript messages posted with `window.wvbridge.postMessageAndReceiveResult(type, options)`.
 */
public fun interface JavaScriptBridgeMessageHandlerWithReply {
    public suspend fun handle(values: List<JSValue>, reply: suspend (JSValue) -> Unit)
}

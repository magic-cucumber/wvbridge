package top.kagg886.wvbridge.js.protocol

/**
 * Handles JavaScript messages posted with `window.wvbridge.postMessage(type, ...messages)`.
 */
public fun interface JavaScriptBridgeMessageHandler {
    public fun handle(vararg values: JSValue)
}

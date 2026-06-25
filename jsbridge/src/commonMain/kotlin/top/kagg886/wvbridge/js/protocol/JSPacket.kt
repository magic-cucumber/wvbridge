package top.kagg886.wvbridge.js.protocol

import top.kagg886.wvbridge.js.internal.JavaScriptBridgePacketPrefix
import top.kagg886.wvbridge.js.internal.base64Decode
import top.kagg886.wvbridge.js.internal.base64Encode

internal data class JSPacket(
    val header: String,
    val type: String,
    val message: JSValue,
) {
    public companion object {
        internal fun String.toJSPacket(): JSPacket {
            val value = removeSurrounding("\"")
            if (!value.startsWith(JavaScriptBridgePacketPrefix)) {
                error("decode failed, packet is $this")
            }

            val payload = value.removePrefix(JavaScriptBridgePacketPrefix)
            val headerSeparator = payload.indexOf(':')
            if (headerSeparator < 0) {
                error("decode failed, packet is $this")
            }

            val typeSeparator = payload.indexOf(':', headerSeparator + 1)
            if (typeSeparator < 0) {
                error("decode failed, packet is $this")
            }

            val header = payload.substring(0, headerSeparator).base64Decode()
            val type = payload.substring(headerSeparator + 1, typeSeparator).base64Decode()
            val message = with(JSValue) {
                payload.substring(typeSeparator + 1).toJavaScriptBridgeValue()
            }

            return JSPacket(
                header = header,
                type = type,
                message = message,
            )
        }

        internal fun JSPacket.toJavaScriptBridgeString(): String = with(JSValue) {
            JavaScriptBridgePacketPrefix +
                header.base64Encode() +
                ":" +
                type.base64Encode() +
                ":" +
                message.toJavaScriptBridgeString()
        }
    }
}

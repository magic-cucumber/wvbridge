package top.kagg886.wvbridge.js.protocol

import kotlinx.serialization.Serializable
import kotlinx.serialization.encodeToString
import top.kagg886.wvbridge.js.internal.JavaScriptBridgePacketHeader
import top.kagg886.wvbridge.js.internal.base64Decode
import top.kagg886.wvbridge.js.internal.base64Encode
import top.kagg886.wvbridge.js.protocol.JSValue.Companion.JsonCodec

@Serializable
internal data class JSPacket(
    val type: String,
    val messages: List<JSValue>,
) {
    internal companion object {
        internal fun String.toJSPacket(): JSPacket {
            val value = unwrapWebViewStringLiteral()
            val separator = value.indexOf(':')
            if (separator < 0) {
                error("decode failed, packet is $this")
            }

            val header = value.substring(0, separator)
            if (header != JavaScriptBridgePacketHeader) {
                error("decode failed, packet is $this")
            }

            val json = value.substring(separator + 1).base64Decode()
            return JsonCodec.decodeFromString(json)
        }
        private fun String.unwrapWebViewStringLiteral(): String {
            return runCatching {
                JsonCodec.decodeFromString<String>(this)
            }.getOrElse {
                this
            }
        }
    }
}

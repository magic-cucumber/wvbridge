package top.kagg886.wvbridge.js.protocol

import top.kagg886.wvbridge.js.internal.JavaScriptBridgeResultPrefix
import top.kagg886.wvbridge.js.internal.base64Decode
import top.kagg886.wvbridge.js.internal.base64Encode

/**
 * The JavaScript evaluation result normalized across native WebView backends.
 */
public sealed interface JSValue {
    /**
     * JavaScript returned `undefined`.
     */
    public data object Undefined : JSValue

    /**
     * JavaScript returned `null`.
     */
    public data object Null : JSValue

    /**
     * A JavaScript object that cannot be faithfully represented by JSON.
     *
     * [type] is the result of JavaScript `Object.prototype.toString.call(value)` for objects
     * and functions, or `typeof value` for primitive values.
     * [value] is the result of JavaScript `String(value)`, for example `console` or `window`.
     */
    public data class ScriptObject(val type: String, val value: String) : JSValue

    /**
     * A JavaScript value serialized with `JSON.stringify`.
     *
     * [value] is the result of JavaScript `JSON.stringify(value)`.
     */
    public data class Serializable(val value: String) : JSValue

    /**
     * JavaScript evaluation threw an exception.
     *
     * [stacktrace] is the JavaScript stack trace when the runtime exposes one.
     */
    public data class Error(val stacktrace: String) : JSValue

    public companion object {
        internal fun String?.toJavaScriptBridgeValue(): JSValue {
            fun parseObject(payload: String): ScriptObject? {
                val separator = payload.indexOf(':')
                if (separator < 0) return null

                val type = payload.substring(0, separator).base64Decode()
                val value = payload.substring(separator + 1).base64Decode()

                return ScriptObject(
                    type = type,
                    value = value
                )
            }

            if (this == null) return Undefined


            val value = run {
                if (length < 2 || first() != '"' || last() != '"') return@run this

                val inner = substring(1, lastIndex)
                if (inner.startsWith(JavaScriptBridgeResultPrefix)) inner else this
            }

            if (!value.startsWith(JavaScriptBridgeResultPrefix)) {
                error("decode failed, result is $this")
            }

            val typeIndex = JavaScriptBridgeResultPrefix.length
            if (value.length < typeIndex + 2 || value[typeIndex + 1] != ':') {
                error("decode failed, result is $this")
            }

            val payload = value.substring(typeIndex + 2)

            return when (value[typeIndex]) {
                'U' -> Undefined
                'N' -> Null
                'S' -> Serializable(
                    payload.base64Decode()
                )

                'O' -> parseObject(payload) ?: error("decode failed, result is $this")
                'E' -> Error(
                    stacktrace = payload.base64Decode()
                )

                else -> error("decode failed, result is $this")
            }
        }

        internal fun JSValue.toJavaScriptBridgeString(): String {
            fun wrap(tag: Char, payload: String = ""): String = "$JavaScriptBridgeResultPrefix$tag:$payload"

            return when (this) {
                Undefined -> wrap('U')
                Null -> wrap('N')
                is Serializable -> wrap('S', value.base64Encode())
                is ScriptObject -> wrap(
                    'O',
                    type.base64Encode() + ":" + value.base64Encode()
                )

                is Error -> wrap('E', stacktrace.base64Encode())
            }
        }
    }
}

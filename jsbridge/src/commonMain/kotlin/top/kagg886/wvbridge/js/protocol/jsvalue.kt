package top.kagg886.wvbridge.js.protocol

import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.SerialName
import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonClassDiscriminator
import kotlinx.serialization.json.JsonElement
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.encodeToJsonElement
import kotlinx.serialization.serializer
import kotlinx.serialization.Serializable as KotlinSerializable
import top.kagg886.wvbridge.js.internal.JavaScriptBridgeValueHeader
import top.kagg886.wvbridge.js.internal.base64Decode
import kotlin.reflect.KClass

/**
 * The JavaScript evaluation result normalized across native WebView backends.
 */
@OptIn(ExperimentalSerializationApi::class)
@KotlinSerializable
@JsonClassDiscriminator("kind")
public sealed interface JSValue {
    /**
     * JavaScript returned `undefined`.
     */
    @KotlinSerializable
    @SerialName("undefined")
    public data object Undefined : JSValue

    /**
     * JavaScript returned `null`.
     */
    @KotlinSerializable
    @SerialName("null")
    public data object Null : JSValue

    /**
     * A JavaScript object that cannot be faithfully represented by JSON.
     *
     * [type] is the result of JavaScript `Object.prototype.toString.call(value)` for objects
     * and functions, or `typeof value` for primitive values.
     * [value] is the result of JavaScript `String(value)`, for example `console` or `window`.
     */
    @KotlinSerializable
    @SerialName("scriptObject")
    public data class ScriptObject(val type: String, val value: String) : JSValue

    /**
     * A JavaScript value serialized with `JSON.stringify`.
     *
     * [value] is the JSON representation of the JavaScript value.
     */
    @KotlinSerializable
    @SerialName("serializable")
    public data class Serializable(val value: JsonObject) : JSValue

    /**
     * JavaScript evaluation threw an exception.
     *
     * [stacktrace] is the JavaScript stack trace when the runtime exposes one.
     */
    @KotlinSerializable
    @SerialName("error")
    public data class Error(val stacktrace: String) : JSValue

    public companion object {
        internal val JsonCodec: Json = Json {
            ignoreUnknownKeys = true
            encodeDefaults = true
        }

        internal fun String?.toJavaScriptBridgeValue(): JSValue {
            if (this == null) return Undefined

            val value = unwrapWebViewStringLiteral()
            val separator = value.indexOf(':')
            if (separator < 0) {
                error("decode failed, result is $this")
            }

            val header = value.substring(0, separator)
            if (header != JavaScriptBridgeValueHeader) {
                error("decode failed, result is $this")
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

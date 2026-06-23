package top.kagg886.wvbridge.bridge

import kotlin.io.encoding.Base64

internal const val JavaScriptBridgeResultPrefix: String = "__wvbridge_result_v1__:"

internal fun buildJavaScriptBridgeEvaluationScript(script: String): String =
    $$"""
        (() => {
            const PREFIX = "$$JavaScriptBridgeResultPrefix";

            const wrap = (tag, payload = "") => PREFIX + tag + ":" + payload;

            const toObjectString = (value) => {
                try {
                    return String(value);
                } catch (_) {
                    return Object.prototype.toString.call(value);
                }
            };

            const encodeBase64UrlUtf8 = (value) =>
                btoa(
                    Array.from(
                        new TextEncoder().encode(value),
                        byte => String.fromCharCode(byte)
                    ).join("")
                )
                    .replaceAll("+", "-")
                    .replaceAll("/", "_")
                    .replace(/=+$/g, "");

            const wrapObject = (value) => {
                const type = typeof value;
                const objectType = value === null || (type !== "object" && type !== "function")
                    ? type
                    : Object.prototype.toString.call(value);

                return wrap(
                    "O",
                    `${encodeBase64UrlUtf8(objectType)}:${encodeBase64UrlUtf8(toObjectString(value))}`
                );
            };

            const isPlainJsonObject = (value) => {
                if (value === null || typeof value !== "object") return false;
                const proto = Object.getPrototypeOf(value);
                return proto === Object.prototype || proto === null;
            };

            const isArrayIndexName = (name) => {
                if (name === "length") return true;

                const index = Number(name);
                return Number.isInteger(index)
                    && index >= 0
                    && index < 4294967295
                    && String(index) === name;
            };

            const isStrictJsonEquivalent = (source, parsed, seen = new WeakSet()) => {
                if (source === null) return parsed === null;

                const type = typeof source;

                if (type === "string" || type === "boolean") {
                    return source === parsed;
                }

                if (type === "number") {
                    return typeof parsed === "number"
                        && Number.isFinite(source)
                        && Object.is(source, parsed);
                }

                if (type !== "object") return false;

                if (seen.has(source)) return false;
                seen.add(source);

                if (Array.isArray(source)) {
                    if (!Array.isArray(parsed)) return false;
                    if (source.length !== parsed.length) return false;
                    if (Object.getOwnPropertySymbols(source).length !== 0) return false;

                    for (const name of Object.getOwnPropertyNames(source)) {
                        if (!isArrayIndexName(name)) return false;
                    }

                    for (let i = 0; i < source.length; i++) {
                        if (!Object.prototype.hasOwnProperty.call(source, i)) return false;

                        const descriptor = Object.getOwnPropertyDescriptor(source, String(i));
                        if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) {
                            return false;
                        }

                        if (!isStrictJsonEquivalent(descriptor.value, parsed[i], seen)) {
                            return false;
                        }
                    }

                    return true;
                }

                if (!isPlainJsonObject(source)) return false;
                if (!isPlainJsonObject(parsed)) return false;
                if (Object.getOwnPropertySymbols(source).length !== 0) return false;

                const sourceNames = Object.getOwnPropertyNames(source);
                const sourceKeys = Object.keys(source);
                const parsedKeys = Object.keys(parsed);

                if (sourceNames.length !== sourceKeys.length) return false;
                if (sourceKeys.length !== parsedKeys.length) return false;

                for (const key of sourceKeys) {
                    const descriptor = Object.getOwnPropertyDescriptor(source, key);
                    if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) {
                        return false;
                    }

                    if (!Object.prototype.hasOwnProperty.call(parsed, key)) {
                        return false;
                    }

                    if (!isStrictJsonEquivalent(descriptor.value, parsed[key], seen)) {
                        return false;
                    }
                }

                return true;
            };

            try {
                const script = new TextDecoder().decode(
                    Uint8Array.from(
                        atob("$${Base64.Default.encode(script.encodeToByteArray())}"),
                        char => char.charCodeAt(0)
                    )
                );

                const value = Function(script).apply(globalThis);

                if (value === undefined) return wrap("U");
                if (value === null) return wrap("N");

                if (typeof value !== "object") {
                    return wrapObject(value);
                }

                let json;
                let parsed;

                try {
                    json = JSON.stringify(value);
                    if (typeof json !== "string") return wrapObject(value);

                    parsed = JSON.parse(json);
                } catch (_) {
                    return wrapObject(value);
                }

                return isStrictJsonEquivalent(value, parsed)
                    ? wrap("S", encodeBase64UrlUtf8(json))
                    : wrapObject(value);
            } catch (error) {
                return wrap(
                    "E",
                    encodeBase64UrlUtf8(
                        error && typeof error.stack === "string"
                            ? error.stack
                            : toObjectString(error)
                    )
                );
            }
        })()
    """.trimIndent()

internal fun String?.toJavaScriptBridgeValue(): JavaScriptBridge.Value {
    if (this == null) return JavaScriptBridge.Value.Undefined


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
        'U' -> JavaScriptBridge.Value.Undefined
        'N' -> JavaScriptBridge.Value.Null
        'S' -> JavaScriptBridge.Value.Serializable(
            decodeBase64UrlUtf8(payload) ?: error("decode failed, result is $this")
        )

        'O' -> parseObject(payload) ?: error("decode failed, result is $this")
        'E' -> JavaScriptBridge.Value.Error(
            stacktrace = decodeBase64UrlUtf8(payload) ?: error("decode failed, result is $this")
        )

        else -> error("decode failed, result is $this")
    }
}

internal fun decodeBase64UrlUtf8(value: String): String? =
    runCatching {
        val padded = value + "=".repeat((4 - value.length % 4) % 4)
        Base64.UrlSafe.decode(padded).decodeToString()
    }.getOrNull()

internal fun parseObject(payload: String): JavaScriptBridge.Value.ScriptObject? {
    val separator = payload.indexOf(':')
    if (separator < 0) return null

    val type = decodeBase64UrlUtf8(payload.substring(0, separator)) ?: return null
    val value = decodeBase64UrlUtf8(payload.substring(separator + 1)) ?: return null

    return JavaScriptBridge.Value.ScriptObject(
        type = type,
        value = value
    )
}

package top.kagg886.wvbridge.js

import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.js.internal.JavaScriptBridgeResultPrefix
import top.kagg886.wvbridge.js.protocol.Value
import kotlin.io.encoding.Base64



/**
 * Evaluates [script] as a JavaScript function body and normalizes the result to [Value].
 *
 * The wrapper sends [script] to the WebView as UTF-8 Base64, decodes it in the page, and executes
 * it with `Function(script).apply(globalThis)`. Because [script] is compiled as a function body,
 * callers must use `return` to produce a value.
 */
public suspend fun JavaScriptBridge.evaluateScriptValue(script: String): Value {
    val script = $$"""
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

            const encodeBase64 = (value) =>
                btoa(
                    Array.from(
                        new TextEncoder().encode(value),
                        byte => String.fromCharCode(byte)
                    ).join("")
                );

            const wrapObject = (value) => {
                const type = typeof value;
                const objectType = value === null || (type !== "object" && type !== "function")
                    ? type
                    : Object.prototype.toString.call(value);

                return wrap(
                    "O",
                    `${encodeBase64(objectType)}:${encodeBase64(toObjectString(value))}`
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
                        atob("$${Base64.encode(script.encodeToByteArray())}"),
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
                    ? wrap("S", encodeBase64(json))
                    : wrapObject(value);
            } catch (error) {
                return wrap(
                    "E",
                    encodeBase64(
                        error && typeof error.stack === "string"
                            ? error.stack
                            : toObjectString(error)
                    )
                );
            }
        })()
    """.trimIndent()

    return with(Value) {
        evaluateScript(script).toJavaScriptBridgeValue()
    }
}

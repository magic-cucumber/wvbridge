package top.kagg886.wvbridge.js.internal

import kotlin.io.encoding.Base64

internal val JavaScriptBridgeValueTemplate: String = """
    const WVB_RESULT_PREFIX = "$JavaScriptBridgeResultPrefix";

    const wvbWrapValue = (tag, payload = "") => WVB_RESULT_PREFIX + tag + ":" + payload;

    const wvbToObjectString = (value) => {
        try {
            return String(value);
        } catch (_) {
            return Object.prototype.toString.call(value);
        }
    };

    const wvbEncodeBase64 = (value) =>
        btoa(
            Array.from(
                new TextEncoder().encode(value),
                byte => String.fromCharCode(byte)
            ).join("")
        );

    const wvbWrapObject = (value) => {
        const type = typeof value;
        const objectType = value === null || (type !== "object" && type !== "function")
            ? type
            : Object.prototype.toString.call(value);

        return wvbWrapValue(
            "O",
            wvbEncodeBase64(objectType) + ":" + wvbEncodeBase64(wvbToObjectString(value))
        );
    };

    const wvbIsPlainJsonObject = (value) => {
        if (value === null || typeof value !== "object") return false;
        const proto = Object.getPrototypeOf(value);
        return proto === Object.prototype || proto === null;
    };

    const wvbIsArrayIndexName = (name) => {
        if (name === "length") return true;

        const index = Number(name);
        return Number.isInteger(index)
            && index >= 0
            && index < 4294967295
            && String(index) === name;
    };

    const wvbIsStrictJsonEquivalent = (source, parsed, seen = new WeakSet()) => {
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
                if (!wvbIsArrayIndexName(name)) return false;
            }

            for (let i = 0; i < source.length; i++) {
                if (!Object.prototype.hasOwnProperty.call(source, i)) return false;

                const descriptor = Object.getOwnPropertyDescriptor(source, String(i));
                if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) {
                    return false;
                }

                if (!wvbIsStrictJsonEquivalent(descriptor.value, parsed[i], seen)) {
                    return false;
                }
            }

            return true;
        }

        if (!wvbIsPlainJsonObject(source)) return false;
        if (!wvbIsPlainJsonObject(parsed)) return false;
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

            if (!wvbIsStrictJsonEquivalent(descriptor.value, parsed[key], seen)) {
                return false;
            }
        }

        return true;
    };

    const wvbToJSValue = (value) => {
        if (value === undefined) return wvbWrapValue("U");
        if (value === null) return wvbWrapValue("N");

        if (typeof value !== "object") {
            return wvbWrapObject(value);
        }

        let json;
        let parsed;

        try {
            json = JSON.stringify(value);
            if (typeof json !== "string") return wvbWrapObject(value);

            parsed = JSON.parse(json);
        } catch (_) {
            return wvbWrapObject(value);
        }

        return wvbIsStrictJsonEquivalent(value, parsed)
            ? wvbWrapValue("S", wvbEncodeBase64(json))
            : wvbWrapObject(value);
    };
""".trimIndent()

internal fun javaScriptBridgeEvaluateScriptTemplate(script: String): String {
    val encodedScript = Base64.encode(script.encodeToByteArray())

    return """
        (() => {
            $JavaScriptBridgeValueTemplate

            try {
                const script = new TextDecoder().decode(
                    Uint8Array.from(
                        atob("$encodedScript"),
                        char => char.charCodeAt(0)
                    )
                );

                return wvbToJSValue(Function(script).apply(globalThis));
            } catch (error) {
                return wvbWrapValue(
                    "E",
                    wvbEncodeBase64(
                        error && typeof error.stack === "string"
                            ? error.stack
                            : wvbToObjectString(error)
                    )
                );
            }
        })()
    """.trimIndent()
}

internal val JavaScriptBridgePostMessageTemplate: String = """
    (() => {
        $JavaScriptBridgeValueTemplate

        const WVB_PACKET_PREFIX = "$JavaScriptBridgePacketPrefix";
        const WVB_PACKET_HEADER = "$JavaScriptBridgePacketHeader";

        window.__wvbridge_jsbridge_initialized__ = true;

        if (window.__wvbridge_jsbridge_initialized__) {
            window.wvbridge = {};

            const wvbPostToNative = (message) => {
                if (window.chrome && window.chrome.webview && typeof window.chrome.webview.postMessage === "function") {
                    window.chrome.webview.postMessage(message);
                    return;
                }

                if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.wvbridge) {
                    window.webkit.messageHandlers.wvbridge.postMessage(message);
                    return;
                }

                if (window._wvbridge && typeof window._wvbridge.postMessage === "function") {
                    window._wvbridge.postMessage(message);
                    return;
                }

                throw new Error("wvbridge native postMessage object is unavailable");
            };

            const wvbToJSPacket = (type, message) =>
                WVB_PACKET_PREFIX +
                    wvbEncodeBase64(WVB_PACKET_HEADER) +
                    ":" +
                    wvbEncodeBase64(String(type)) +
                    ":" +
                    wvbToJSValue(message);

            window.wvbridge.postMessage = (type, message = {}) => {
                wvbPostToNative(wvbToJSPacket(type, message));
            };
        }
    })()
""".trimIndent()

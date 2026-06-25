package top.kagg886.wvbridge.js.internal

import top.kagg886.wvbridge.js.protocol.JSValue
import kotlin.io.encoding.Base64

internal val JavaScriptBridgeValueTemplate: String = """
    const wvb_result_prefix = "$JavaScriptBridgeResultPrefix";

    const wvb_wrap_value = (tag, payload = "") => wvb_result_prefix + tag + ":" + payload;

    const wvb_to_object_string = (value) => {
        try {
            return String(value);
        } catch (_) {
            return Object.prototype.toString.call(value);
        }
    };

    const wvb_encode_base64 = (value) =>
        btoa(
            Array.from(
                new TextEncoder().encode(value),
                byte => String.fromCharCode(byte)
            ).join("")
        );

    const wvb_decode_base64 = (value) =>
        new TextDecoder().decode(
            Uint8Array.from(
                atob(value),
                char => char.charCodeAt(0)
            )
        );

    const wvb_wrap_object = (value) => {
        const type = typeof value;
        const object_type = value === null || (type !== "object" && type !== "function")
            ? type
            : Object.prototype.toString.call(value);

        return wvb_wrap_value(
            "O",
            wvb_encode_base64(object_type) + ":" + wvb_encode_base64(wvb_to_object_string(value))
        );
    };

    const wvb_is_plain_json_object = (value) => {
        if (value === null || typeof value !== "object") return false;
        const proto = Object.getPrototypeOf(value);
        return proto === Object.prototype || proto === null;
    };

    const wvb_is_array_index_name = (name) => {
        if (name === "length") return true;

        const index = Number(name);
        return Number.isInteger(index)
            && index >= 0
            && index < 4294967295
            && String(index) === name;
    };

    const wvb_is_strict_json_equivalent = (source, parsed, seen = new WeakSet()) => {
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
                if (!wvb_is_array_index_name(name)) return false;
            }

            for (let i = 0; i < source.length; i++) {
                if (!Object.prototype.hasOwnProperty.call(source, i)) return false;

                const descriptor = Object.getOwnPropertyDescriptor(source, String(i));
                if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) {
                    return false;
                }

                if (!wvb_is_strict_json_equivalent(descriptor.value, parsed[i], seen)) {
                    return false;
                }
            }

            return true;
        }

        if (!wvb_is_plain_json_object(source)) return false;
        if (!wvb_is_plain_json_object(parsed)) return false;
        if (Object.getOwnPropertySymbols(source).length !== 0) return false;

        const source_names = Object.getOwnPropertyNames(source);
        const source_keys = Object.keys(source);
        const parsed_keys = Object.keys(parsed);

        if (source_names.length !== source_keys.length) return false;
        if (source_keys.length !== parsed_keys.length) return false;

        for (const key of source_keys) {
            const descriptor = Object.getOwnPropertyDescriptor(source, key);
            if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) {
                return false;
            }

            if (!Object.prototype.hasOwnProperty.call(parsed, key)) {
                return false;
            }

            if (!wvb_is_strict_json_equivalent(descriptor.value, parsed[key], seen)) {
                return false;
            }
        }

        return true;
    };

    const wvb_to_js_value = (value) => {
        if (value === undefined) return wvb_wrap_value("U");
        if (value === null) return wvb_wrap_value("N");

        if (typeof value !== "object") {
            return wvb_wrap_object(value);
        }

        let json;
        let parsed;

        try {
            json = JSON.stringify(value);
            if (typeof json !== "string") return wvb_wrap_object(value);

            parsed = JSON.parse(json);
        } catch (_) {
            return wvb_wrap_object(value);
        }

        return wvb_is_strict_json_equivalent(value, parsed)
            ? wvb_wrap_value("S", wvb_encode_base64(json))
            : wvb_wrap_object(value);
    };
""".trimIndent()

internal fun javaScriptBridgeEvaluateScriptTemplate(script: String): String {
    val encodedScript = Base64.encode(script.encodeToByteArray())

    return """
        (() => {
            $JavaScriptBridgeValueTemplate

            try {
                const script = wvb_decode_base64("$encodedScript");

                return wvb_to_js_value(Function(script).apply(globalThis));
            } catch (error) {
                return wvb_wrap_value(
                    "E",
                    wvb_encode_base64(
                        error && typeof error.stack === "string"
                            ? error.stack
                            : wvb_to_object_string(error)
                    )
                );
            }
        })()
    """.trimIndent()
}

internal val JavaScriptBridgePostMessageTemplate: String = """
    (() => {
        $JavaScriptBridgeValueTemplate

        const wvb_packet_prefix = "$JavaScriptBridgePacketPrefix";
        const wvb_packet_header = "$JavaScriptBridgePacketHeader";

        if (!window.__wvbridge_jsbridge_initialized__) {
            window.wvbridge = window.wvbridge || {};
            const wvb_message_listeners = new Map();

            const wvb_post_to_native = (message) => {
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

            const wvb_to_js_packet = (type, message) =>
                wvb_packet_prefix +
                    wvb_encode_base64(wvb_packet_header) +
                    ":" +
                    wvb_encode_base64(String(type)) +
                    ":" +
                    wvb_to_js_value(message);

            window.wvbridge.postMessage = (type, message = {}) => {
                wvb_post_to_native(wvb_to_js_packet(type, message));
            };

            const wvb_get_message_listeners = (type, create_if_absent = false) => {
                const key = String(type);
                let listeners = wvb_message_listeners.get(key);

                if (!listeners && create_if_absent) {
                    listeners = new Set();
                    wvb_message_listeners.set(key, listeners);
                }

                return listeners;
            };

            window.wvbridge.addEventListener = (type, listener) => {
                if (typeof listener !== "function") {
                    throw new TypeError("wvbridge event listener must be a function");
                }

                wvb_get_message_listeners(type, true).add(listener);
            };

            window.wvbridge.removeEventListener = (type, listener) => {
                const listeners = wvb_get_message_listeners(type);
                if (!listeners) return;

                listeners.delete(listener);

                if (listeners.size === 0) {
                    wvb_message_listeners.delete(String(type));
                }
            };

            window.wvbridge.dispatchEvent = (type, message) => {
                const listeners = wvb_get_message_listeners(type);
                if (!listeners) return false;

                for (const listener of Array.from(listeners)) {
                    listener.call(window.wvbridge, message, String(type));
                }

                return true;
            };

            window.__wvbridge_jsbridge_initialized__ = true;
        }
    })()
""".trimIndent()

internal fun javaScriptBridgePostMessageDispatchTemplate(type: String, value: JSValue): String {
    val valueExpression = when (value) {
        JSValue.Undefined -> "undefined"
        JSValue.Null -> "null"
        is JSValue.Serializable -> "JSON.parse(wvb_decode_base64(\"${value.value.base64Encode()}\"))"
        is JSValue.ScriptObject,
        is JSValue.Error -> throw IllegalArgumentException("JavaScriptBridge.postMessage only supports JSValue.Undefined, JSValue.Null, and JSValue.Serializable")
    }

    return """
        (() => {
            $JavaScriptBridgeValueTemplate

            window.wvbridge.dispatchEvent(
                wvb_decode_base64("${type.base64Encode()}"),
                $valueExpression
            );
        })()
    """.trimIndent()
}

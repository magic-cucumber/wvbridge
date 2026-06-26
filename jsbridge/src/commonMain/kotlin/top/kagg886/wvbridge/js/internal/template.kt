package top.kagg886.wvbridge.js.internal

internal val WebViewBridgeExtInstallScript: String = iife(
    script = """
        if (window.__wvbridge__ !== undefined) return;
        const encodeBase64 = (value) =>
            btoa(
                Array.from(
                    new TextEncoder().encode(value),
                    byte => String.fromCharCode(byte)
                ).join("")
            );

        const decodeBase64 = (value) =>
            new TextDecoder().decode(
                Uint8Array.from(
                    atob(value),
                    char => char.charCodeAt(0)
                )
            );

        const toObjectString = (value) => {
            try {
                return String(value);
            } catch (_) {
                return Object.prototype.toString.call(value);
            }
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

            if (type === "string" || type === "boolean") return source === parsed;

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
                    if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) return false;

                    if (!isStrictJsonEquivalent(descriptor.value, parsed[i], seen)) return false;
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
                if (!descriptor || !descriptor.enumerable || !("value" in descriptor)) return false;
                if (!Object.prototype.hasOwnProperty.call(parsed, key)) return false;
                if (!isStrictJsonEquivalent(descriptor.value, parsed[key], seen)) return false;
            }

            return true;
        };

        const toScriptObjectValueObject = (value) => {
            const type = typeof value;
            const objectType = value === null || (type !== "object" && type !== "function")
                ? type
                : Object.prototype.toString.call(value);

            return {
                kind: "scriptObject",
                type: objectType,
                value: toObjectString(value)
            };
        };

        const toErrorValueObject = (error) => ({
            kind: "error",
            stacktrace: error && typeof error.stack === "string"
                ? error.stack
                : toObjectString(error)
        });

        const toJSValueObject = (value) => {
            if (value === undefined) return { kind: "undefined" };
            if (value === null) return { kind: "null" };

            if (typeof value !== "object") return toScriptObjectValueObject(value);

            let json;
            let parsed;

            try {
                json = JSON.stringify(value);
                if (typeof json !== "string") return toScriptObjectValueObject(value);

                parsed = JSON.parse(json);
            } catch (_) {
                return toScriptObjectValueObject(value);
            }

            return isStrictJsonEquivalent(value, parsed)
                ? { kind: "serializable", value: parsed }
                : toScriptObjectValueObject(value);
        };

        const messageListeners = new Map();
        const wvbridge = window.wvbridge || {};

        const bridge = {
            valueHeader: "$JavaScriptBridgeValueHeader",
            packetHeader: "$JavaScriptBridgePacketHeader",
            encodeBase64,
            decodeBase64,
            toErrorValueObject,
            toJSValueObject,
            wrapWire: (header, payload) => header + ":" + encodeBase64(JSON.stringify(payload)),
            toPacketWithValue: (type, message) => bridge.wrapWire(
                bridge.packetHeader,
                { type: String(type), message }
            ),
            toPacket: (type, message) => bridge.toPacketWithValue(type, toJSValueObject(message)),
            postToNative: (message) => {
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
            }
        };

        const getMessageListeners = (type, createIfAbsent = false) => {
            const key = String(type);
            let listeners = messageListeners.get(key);

            if (!listeners && createIfAbsent) {
                listeners = new Set();
                messageListeners.set(key, listeners);
            }

            return listeners;
        };

        wvbridge.postMessage = (type, message = {}) => {
            bridge.postToNative(bridge.toPacket(type, message));
        };

        wvbridge.addEventListener = (type, listener) => {
            if (typeof listener !== "function") {
                throw new TypeError("wvbridge event listener must be a function");
            }

            getMessageListeners(type, true).add(listener);
        };

        wvbridge.removeEventListener = (type, listener) => {
            const listeners = getMessageListeners(type);
            if (!listeners) return;

            listeners.delete(listener);

            if (listeners.size === 0) {
                messageListeners.delete(String(type));
            }
        };

        wvbridge.dispatchEvent = (type, message) => {
            const listeners = getMessageListeners(type);
            if (!listeners) return false;

            for (const listener of Array.from(listeners)) {
                listener.call(wvbridge, message, String(type));
            }

            return true;
        };

        window.wvbridge = wvbridge;
        window.__wvbridge__ = bridge;
""".trimIndent()
)

internal fun iife(script: String) = """
    (() => {
        $script
    })()
""".trimIndent()

package top.kagg886.wvbridge.js.internal

/**
 * Installs the JavaScript-side helper used by `wvbridge:jsbridge`.
 *
 * The script is wrapped as an IIFE and is idempotent: if `window.__wvbridge__` already exists,
 * it returns immediately. Installation exposes two global objects:
 *
 * - `window.wvbridge`: public page-facing API.
 * - `window.__wvbridge__`: internal helper object used by Kotlin extension functions and the
 *   injected script itself. Page code should not depend on it as a stable public API.
 *
 * Public injected API on `window.wvbridge`:
 *
 * | Method | Purpose | Parameters | Return value |
 * |--------|---------|------------|--------------|
 * | `postMessage(type, message = {})` | Sends a typed packet from JavaScript to native code. Kotlin receives it through `JavaScriptBridge.registerWebMessageHandler(type)`. | `type`: packet type, converted with `String(type)`. `message`: payload, default `{}`. The value is normalized to a `JSValue` shape before being sent. | `undefined`. Throws when no native postMessage transport is available. |
 * | `addEventListener(type, listener)` | Registers a JavaScript listener for messages dispatched from native code with `JavaScriptBridge.postMessage(type, ...values)`. | `type`: event type, converted with `String(type)`. `listener`: function called as `listener.call(window.wvbridge, ...args)`. | `undefined`. Throws `TypeError` if `listener` is not a function. |
 * | `removeEventListener(type, listener)` | Removes a previously registered listener for the given type. | `type`: event type. `listener`: the same function reference passed to `addEventListener`. | `undefined`. Missing listeners are ignored. |
 * | `dispatchEvent(type, ...args)` | Dispatches an event to JavaScript listeners registered for `type`. It is mainly called by native-side `JavaScriptBridge.postMessage`. | `type`: event type. `args`: values delivered to listeners. If no args are provided, listeners receive one `undefined` argument. | `true` when listeners existed and were called, otherwise `false`. |
 *
 * Internal injected API on `window.__wvbridge__`:
 *
 * | Member | Purpose | Parameters | Return value |
 * |--------|---------|------------|--------------|
 * | `valueHeader` | Wire header for values returned by `evaluateScriptValue`. | None. | `"wvbridge-js-value-v1"`. |
 * | `packetHeader` | Wire header for message packets sent from JavaScript to native code. | None. | `"wvbridge-js-packet-v1"`. |
 * | `encodeBase64(value)` | Encodes a UTF-8 string for transport. | `value`: string. | Base64 string. |
 * | `decodeBase64(value)` | Decodes a UTF-8 Base64 string. | `value`: Base64 string. | Decoded string. |
 * | `toErrorValueObject(error)` | Converts a thrown JavaScript error to a `JSValue.Error`-compatible object. | `error`: any thrown value. | `{ kind: "error", stacktrace: string }`. |
 * | `toJSValueObject(value)` | Normalizes a JavaScript value to the JSON model decoded by Kotlin `JSValue`. | `value`: any JavaScript value. | One of `{ kind: "undefined" }`, `{ kind: "null" }`, `{ kind: "serializable", value }`, or `{ kind: "scriptObject", type, value }`. |
 * | `wrapWire(header, payload)` | Builds the string wire format used by Kotlin decoders. | `header`: protocol header. `payload`: JSON-serializable object. | `"<header>:<base64(json)>"`. |
 * | `toPacketWithValue(type, message)` | Builds a packet using an already-normalized `JSValue` object. | `type`: packet type. `message`: normalized `JSValue` object. | Wire string with `packetHeader`. |
 * | `toPacket(type, message)` | Builds a packet from an arbitrary JavaScript value. | `type`: packet type. `message`: any JavaScript value. | Wire string with `packetHeader`. |
 * | `postToNative(message)` | Sends a raw wire string through the platform WebView bridge. It tries WebView2, WebKit, then AndroidX WebKit transports. | `message`: raw string to send. | `undefined`. Throws if no supported transport exists. |
 */
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

        wvbridge.dispatchEvent = (type, ...args) => {
            const listeners = getMessageListeners(type);
            if (!listeners) return false;

            const parameters = args.length === 0 ? [undefined] : args;

            for (const listener of Array.from(listeners)) {
                listener.call(wvbridge, ...parameters);
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

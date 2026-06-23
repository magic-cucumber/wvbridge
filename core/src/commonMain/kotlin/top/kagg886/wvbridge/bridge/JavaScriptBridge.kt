package top.kagg886.wvbridge.bridge

/**
 * Executes JavaScript against the current page and installs scripts that should run before the
 * page's own JavaScript.
 *
 * Platform implementations map this API to the native WebView bridge:
 * - Android uses `WebView.evaluateJavascript` and AndroidX WebKit document-start scripts.
 * - iOS and macOS use WebKit user scripts injected at document start.
 * - Desktop/JVM forwards calls to the native backend through `WebViewBridgePanel`.
 */
public interface JavaScriptBridge {
    /**
     * Evaluates [script] in the current page context.
     *
     * The common wrapper sends [script] to the WebView as UTF-8 Base64, decodes it in the page,
     * and executes it with `Function(script).apply(globalThis)`. This avoids direct `eval` and
     * prevents the evaluated code from capturing the wrapper's local variables, but it is still
     * dynamic JavaScript execution with the page's privileges. Only pass trusted code. Pages with
     * a Content Security Policy that disallows `unsafe-eval` may also block this execution path.
     *
     * Because [script] is compiled as a function body, callers must use `return` to produce a
     * value. For example, use `return ({ a: 1 })` instead of `{ a: 1 }`. The latter is parsed as a
     * statement block and returns `undefined`.
     *
     * The returned [Value] is the platform's serialized representation of the JavaScript result.
     * Implementations may throw when the native WebView reports an evaluation error.
     */
    public suspend fun evaluateScript(script: String): Value

    /**
     * Registers [script] so it runs at document start for future page loads.
     *
     * The returned [CloseHandle] unregisters this hook. Platforms that cannot remove one native
     * script directly emulate removal by maintaining their own hook registry and rebuilding the
     * installed document-start scripts.
     */
    public suspend fun registerDocumentStartHook(script: String): CloseHandle

    /**
     * The JavaScript evaluation result normalized across native WebView backends.
     */
    public sealed interface Value {
        /**
         * JavaScript returned `undefined`.
         */
        public data object Undefined : Value

        /**
         * JavaScript returned `null`.
         */
        public data object Null : Value

        /**
         * A JavaScript object that cannot be faithfully represented by JSON.
         *
         * [type] is the result of JavaScript `Object.prototype.toString.call(value)` for objects
         * and functions, or `typeof value` for primitive values.
         * [value] is the result of JavaScript `String(value)`, for example `console` or `window`.
         */
        public data class ScriptObject(val type: String, val value: String) : Value

        /**
         * A JavaScript value serialized with `JSON.stringify`.
         *
         * [value] is the result of JavaScript `JSON.stringify(value)`.
         */
        public data class Serializable(val value: String) : Value

        /**
         * JavaScript evaluation threw an exception.
         *
         * [stacktrace] is the JavaScript stack trace when the runtime exposes one.
         */
        public data class Error(val stacktrace: String) : Value
    }
}

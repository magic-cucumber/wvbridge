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
         * A backend-provided serialized JavaScript value.
         */
        public data class ScriptObject(val value: String) : Value
    }
}

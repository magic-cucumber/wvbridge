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
     * The returned value is the platform's string representation of the JavaScript result.
     * Implementations may throw when the native WebView reports an evaluation error.
     */
    public suspend fun evaluateScript(script: String): String?

    /**
     * Registers [script] so it runs at document start for future page loads.
     *
     * The returned [CloseHandle] unregisters this hook. Platforms that cannot remove one native
     * script directly emulate removal by maintaining their own hook registry and rebuilding the
     * installed document-start scripts.
     */
    public suspend fun registerDocumentStartHook(script: String): CloseHandle

    /**
     * Registers a native web-message handler for the current platform.
     *
     * Page scripts send a string message through the platform's native `postMessage` entry point,
     * and the message is delivered to [handler].
     *
     * Platform JavaScript entry points:
     *
     * | Platform | JavaScript call |
     * | --- | --- |
     * | WebView2 / Windows desktop | `window.chrome.webview.postMessage("hello")` |
     * | WebKitGTK / Linux desktop / Apple | `window.webkit.messageHandlers.wvbridge.postMessage("hello")` |
     * | AndroidX WebKit | `window._wvbridge.postMessage("hello")` |
     *
     * Limitations:
     *
     * - The common API delivers messages as [String] values. If a platform supports JSON or other
     *   structured values, callers should serialize them to a string before posting.
     * - The exact JavaScript global object shape is platform-defined and is not normalized by
     *   core. WebView2 has a single `window.chrome.webview.postMessage` channel, while WebKit and
     *   AndroidX expose the fixed `wvbridge` handler/object.
     * - A handler is available only after the native platform has registered it. Register document
     *   start hooks separately when page scripts need the bridge before normal page JavaScript runs.
     * - Android depends on AndroidX WebKit `WEB_MESSAGE_LISTENER` support. Platforms or WebView
     *   versions without the required native feature may throw [UnsupportedOperationException].
     * - Origin, frame, and security restrictions are platform-specific. AndroidX WebKit uses allowed
     *   origin rules; WebKit-style handlers are scoped to the configured content controller; WebView2
     *   receives messages from the current WebView content.
     * - The returned [CloseHandle] unregisters this native handler. Platform-provided JavaScript
     *   objects may still exist after [CloseHandle.close], but messages are no longer delivered to
     *   this handler.
     */
    public suspend fun registerWebMessageHandler(handler: WebMessageConsumer): CloseHandle
}

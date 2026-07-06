package top.kagg886.wvbridge.internal.listener

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import top.kagg886.wvbridge.util.LoggerReceiver
import java.util.concurrent.ConcurrentHashMap
import javax.swing.SwingUtilities

internal object NativeBridge {
    private val panels = ConcurrentHashMap.newKeySet<WebViewBridgePanel>()

    fun register(panel: WebViewBridgePanel) {
        check(panel.handle != 0L) { "Cannot register a WebViewBridgePanel without a native handle" }
        check(panels.add(panel)) { "WebViewBridgePanel is already registered" }
    }

    fun unregister(panel: WebViewBridgePanel) {
        panels.remove(panel)
    }

    private fun findPanel(webview: Long): WebViewBridgePanel? =
        panels.firstOrNull { it.handle == webview }

    @JvmStatic
    private fun onPageLoadingStartCallback(webview: Long, url: String) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onPageLoadingStartCallback: webview=$webview url=$url")
        findPanel(webview)?.pageLoadingStartListener?.forEach { it.accept(url) }
    }

    @JvmStatic
    private fun onPageLoadingProgressCallback(webview: Long, progress: Float) {
        findPanel(webview)?.pageLoadingProgressListener?.forEach { it.accept(progress) }
    }

    @JvmStatic
    private fun onPageLoadingEndCallback(webview: Long, success: Boolean, reason: String?) {
        findPanel(webview)?.pageLoadingEndListener?.forEach { it.accept(success, reason) }
    }

    @JvmStatic
    private fun onURLChangeCallback(webview: Long, url: String) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onURLChangeCallback: webview=$webview url=$url")
        findPanel(webview)?.urlChangeListener?.forEach { it.accept(url) }
    }

    @JvmStatic
    private fun onNavigationInterceptorCallback(webview: Long, url: String): String {
        val result = findPanel(webview)?.navigationInterceptor?.invoke(url) ?: "1"
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onNavigationInterceptorCallback: webview=$webview url=$url result=$result")
        return result
    }

    @JvmStatic
    private fun onCanGoBackChangeCallback(webview: Long, canGoBack: Boolean) {
        findPanel(webview)?.canGoBackChangeListener?.forEach { it.accept(canGoBack) }
    }

    @JvmStatic
    private fun onCanGoForwardChangeCallback(webview: Long, canGoForward: Boolean) {
        findPanel(webview)?.canGoForwardChangeListener?.forEach { it.accept(canGoForward) }
    }

    @JvmStatic
    private fun onWebViewFatalErrorOccurred(webview: Long, cause: String?) {
        val panel = findPanel(webview) ?: return
        SwingUtilities.invokeLater {
            if (findPanel(webview) === panel) {
                panel.close(cause)
            }
        }
    }

    @JvmStatic
    private fun onNativeLoggerPostedCallback(level: String, tag: String, message: String): Unit = LoggerReceiver.log(
        LoggerReceiver.Level.valueOf(level), tag, message
    )

    private const val TAG = "NativeBridge"
}

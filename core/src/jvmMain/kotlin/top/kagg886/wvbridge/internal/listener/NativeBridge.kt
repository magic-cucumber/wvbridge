package top.kagg886.wvbridge.internal.listener

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.util.concurrent.ConcurrentHashMap

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
        findPanel(webview)?.urlChangeListener?.forEach { it.accept(url) }
    }

    @JvmStatic
    private fun onCanGoBackChangeCallback(webview: Long, canGoBack: Boolean) {
        findPanel(webview)?.canGoBackChangeListener?.forEach { it.accept(canGoBack) }
    }

    @JvmStatic
    private fun onCanGoForwardChangeCallback(webview: Long, canGoForward: Boolean) {
        findPanel(webview)?.canGoForwardChangeListener?.forEach { it.accept(canGoForward) }
    }
}

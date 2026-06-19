package top.kagg886.wvbridge.internal.listener

import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.util.concurrent.ConcurrentHashMap

internal fun interface PageLoadingStartListener {
    fun onPageLoadingStart(webview: Long, url: String)
}

internal fun interface PageLoadingProgressListener {
    fun onPageLoadingProgress(webview: Long, progress: Float)
}

internal fun interface PageLoadingEndListener {
    fun onPageLoadingEnd(webview: Long, success: Boolean, reason: String?)
}

internal fun interface URLChangeListener {
    fun onURLChange(webview: Long, url: String)
}

internal fun interface CanGoBackChangeListener {
    fun onCanGoBackChange(webview: Long, canGoBack: Boolean)
}

internal fun interface CanGoForwardChangeListener {
    fun onCanGoForwardChange(webview: Long, canGoForward: Boolean)
}

internal object NativeBridge {
    private val panels = ConcurrentHashMap.newKeySet<WebViewBridgePanel>()

    init {
        setPageLoadingStartListener { webview, url ->
            findPanel(webview)?.pageLoadingStartListener?.forEach { it.accept(url) }
        }
        setPageLoadingProgressListener { webview, progress ->
            findPanel(webview)?.pageLoadingProgressListener?.forEach { it.accept(progress) }
        }
        setPageLoadingEndListener { webview, success, reason ->
            findPanel(webview)?.pageLoadingEndListener?.forEach { it.accept(success, reason) }
        }
        setURLChangeListener { webview, url ->
            findPanel(webview)?.urlChangeListener?.forEach { it.accept(url) }
        }
        setCanGoBackChangeListener { webview, canGoBack ->
            findPanel(webview)?.canGoBackChangeListener?.forEach { it.accept(canGoBack) }
        }
        setCanGoForwardChangeListener { webview, canGoForward ->
            findPanel(webview)?.canGoForwardChangeListener?.forEach { it.accept(canGoForward) }
        }
    }

    fun register(panel: WebViewBridgePanel) {
        check(panel.handle != 0L) { "Cannot register a WebViewBridgePanel without a native handle" }
        check(panels.add(panel)) { "WebViewBridgePanel is already registered" }
    }

    fun unregister(panel: WebViewBridgePanel) {
        panels.remove(panel)
    }

    private fun findPanel(webview: Long): WebViewBridgePanel? =
        panels.firstOrNull { it.handle == webview }

    private external fun setPageLoadingStartListener(listener: PageLoadingStartListener)
    private external fun setPageLoadingProgressListener(listener: PageLoadingProgressListener)
    private external fun setPageLoadingEndListener(listener: PageLoadingEndListener)
    private external fun setURLChangeListener(listener: URLChangeListener)
    private external fun setCanGoBackChangeListener(listener: CanGoBackChangeListener)
    private external fun setCanGoForwardChangeListener(listener: CanGoForwardChangeListener)
}

package top.kagg886.wvbridge

import androidx.compose.runtime.*
import kotlinx.coroutines.suspendCancellableCoroutine
import platform.Foundation.NSError
import platform.Foundation.NSURL
import platform.Foundation.NSURLRequest
import platform.WebKit.WKUserScript
import platform.WebKit.WKUserScriptInjectionTime
import platform.WebKit.WKWebView
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

internal class AutoClosableWKWebView(public val delegate: WKWebView) : AutoCloseable {
    override fun close(): Unit = Unit
}

internal class WKWebViewController(instance: AutoClosableWKWebView) : WebViewController<AutoClosableWKWebView>(instance) {
    internal val _bridge by lazy {
        WKJavaScriptBridge(instance.delegate)
    }
    internal val _navigator by lazy {
        WKWebViewNavigator(instance.delegate)
    }
    override val navigator: WebViewNavigator
        get() = _navigator
    override val bridge: JavaScriptBridge
        get() = _bridge

}

internal class WKJavaScriptBridge(private val instance: WKWebView) : JavaScriptBridge {
    private var nextDocumentStartHookId = 0L
    private val documentStartHooks = linkedMapOf<Long, String>()

    override suspend fun evaluateScript(script: String): String? =
        suspendCancellableCoroutine { continuation ->
            instance.evaluateJavaScript(script) { result, error ->
                if (error != null) {
                    continuation.resumeWithException(error.toException())
                } else {
                    continuation.resume(result?.toString())
                }
            }
        }

    override suspend fun registerDocumentStartHook(script: String): CloseHandle {
        val id = nextDocumentStartHookId++
        documentStartHooks[id] = script
        addDocumentStartUserScript(script)

        return object : CloseHandle {
            private var closed = false

            override fun close() {
                if (closed) return
                closed = true
                documentStartHooks.remove(id)
                rebuildDocumentStartUserScripts()
            }
        }
    }

    private fun rebuildDocumentStartUserScripts() {
        instance.configuration.userContentController.removeAllUserScripts()
        documentStartHooks.values.forEach(::addDocumentStartUserScript)
    }

    private fun addDocumentStartUserScript(script: String) {
        val userScript = WKUserScript(
            source = script,
            injectionTime = WKUserScriptInjectionTime.WKUserScriptInjectionTimeAtDocumentStart,
            forMainFrameOnly = false,
        )
        instance.configuration.userContentController.addUserScript(userScript)
    }

    private fun NSError.toException(): RuntimeException =
        RuntimeException("WKWebView JavaScript evaluation failed: $localizedDescription")
}

internal class WKWebViewNavigator(private val instance: WKWebView) : WebViewNavigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean {
        instance.goBack()
        return instance.canGoBack()
    }

    override fun goForward(): Boolean {
        instance.goForward()
        return instance.canGoForward()
    }

    override fun refresh() {
        instance.reload()
    }

    override fun stop(): Unit = instance.stopLoading()

    override fun loadUrl(url: String) {
        instance.loadRequest(NSURLRequest.requestWithURL(NSURL.URLWithString(url)!!))
    }
}

@Composable
public actual fun rememberWebViewController(url: String): WebViewController<*> {
    val controller = remember {
        val wv = WKWebView()
        wv.configuration.defaultWebpagePreferences.allowsContentJavaScript = true
        wv.configuration.preferences.javaScriptCanOpenWindowsAutomatically = true

        WKWebViewController(AutoClosableWKWebView(wv))
    }

    LaunchedEffect(Unit) {
        controller.url = url
        controller.loadingState = LoadingState.Ready
    }

    return controller
}

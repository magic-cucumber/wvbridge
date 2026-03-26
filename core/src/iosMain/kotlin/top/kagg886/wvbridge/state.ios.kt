package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import platform.Foundation.NSURL
import platform.Foundation.NSURLRequest
import platform.WebKit.WKWebView
import platform.WebKit.WKWebViewConfiguration

public class AutoClosableWKWebView(public val delegate: WKWebView) : AutoCloseable {
    override fun close(): Unit = Unit
}

public class WKWebViewState(instance: AutoClosableWKWebView) : WebViewState<AutoClosableWKWebView>(instance) {
    internal val _navigator by lazy {
        WKWebViewNavigator(instance.delegate)
    }
    override val navigator: WebViewNavigator
        get() = _navigator

}

public class WKWebViewNavigator(private val instance: WKWebView) : WebViewNavigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean {
        instance.goBack()
        return instance.canGoBack()
    }

    override fun goForward(url: String): Boolean {
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
public actual fun rememberWebViewState(url: String): WebViewState<*> {
    val state = remember {
        val wv = WKWebView()
        wv.configuration.defaultWebpagePreferences.allowsContentJavaScript = true
        wv.configuration.preferences.javaScriptCanOpenWindowsAutomatically = true

        WKWebViewState(AutoClosableWKWebView(wv))
    }

    LaunchedEffect(Unit) {
        state.url = url
        state.state = LoadingState.Ready
    }

    return state
}

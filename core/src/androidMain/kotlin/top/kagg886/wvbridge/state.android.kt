package top.kagg886.wvbridge

import android.webkit.WebView
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext

@JvmInline
public value class AutoClosableWebView(public val instance: WebView) : AutoCloseable {
    override fun close(): Unit = Unit
}

public class AndroidWebViewState(delegate: AutoClosableWebView) : WebViewState<AutoClosableWebView>(delegate) {
    internal val _navigator by lazy {
        AndroidWebViewNavigator(delegate.instance)
    }
    override val navigator: WebViewNavigator
        get() = _navigator
}

public class AndroidWebViewNavigator(private val instance: WebView) : WebViewNavigator {
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

    override fun refresh(): Unit = instance.reload()
    override fun stop(): Unit = instance.stopLoading()

    override fun loadUrl(url: String): Unit = instance.loadUrl(url)
}

@Composable
public actual fun rememberWebViewState(url: String): WebViewState<*> {
    val ctx = LocalContext.current

    val state = remember {
        val wv = WebView(ctx)
        wv.settings.javaScriptEnabled = true
        wv.settings.domStorageEnabled = true
        wv.settings.javaScriptCanOpenWindowsAutomatically = true
        wv.settings.loadsImagesAutomatically = true
        AndroidWebViewState(AutoClosableWebView(wv))
    }

    LaunchedEffect(Unit) {
        state.url = url
        state.state = LoadingState.Ready
    }

    return state
}

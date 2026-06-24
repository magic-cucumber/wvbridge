package top.kagg886.wvbridge

import android.annotation.SuppressLint
import android.os.Handler
import android.os.Looper
import android.webkit.WebView
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.coroutines.startCoroutine
import kotlin.coroutines.suspendCoroutine
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge

@JvmInline
internal value class AutoClosableWebView(public val instance: WebView) : AutoCloseable {
    override fun close(): Unit = Unit
}

internal class AndroidWebViewController(delegate: AutoClosableWebView) : WebViewController<AutoClosableWebView>(delegate) {
    internal val _bridge by lazy {
        AndroidJavaScriptBridge(delegate.instance)
    }
    internal val _navigator by lazy {
        AndroidWebViewNavigator(delegate.instance)
    }
    override val navigator: WebViewNavigator
        get() = _navigator
    override val bridge: JavaScriptBridge
        get() = _bridge
}

internal class AndroidJavaScriptBridge(private val instance: WebView) : JavaScriptBridge {
    private val mainHandler = Handler(Looper.getMainLooper())

    override suspend fun evaluateScript(script: String): String? =
        onMainThread {
            suspendCancellableCoroutine { continuation ->
                instance.evaluateJavascript(script) { result ->
                    continuation.resume(result)
                }
            }
        }

    override suspend fun registerDocumentStartHook(script: String): CloseHandle =
        onMainThread {
            if (!WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
                throw UnsupportedOperationException("Android WebView document-start script injection is not supported")
            }

            val handler = WebViewCompat.addDocumentStartJavaScript(instance, script, setOf("*"))
            object : CloseHandle {
                private var closed = false

                override fun close() {
                    if (closed) return
                    closed = true
                    runOnMainThread {
                        handler.remove()
                    }
                }
            }
        }

    private suspend fun <T> onMainThread(block: suspend () -> T): T {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            return block()
        }
        return suspendCancellableCoroutine { continuation ->
            mainHandler.post {
                block.startCoroutine(continuation)
            }
        }
    }

    private fun runOnMainThread(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            block()
        } else {
            mainHandler.post(block)
        }
    }

}

internal class AndroidWebViewNavigator(private val instance: WebView) : WebViewNavigator {
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

    override fun refresh(): Unit = instance.reload()
    override fun stop(): Unit = instance.stopLoading()

    override fun loadUrl(url: String): Unit = instance.loadUrl(url)
}

@SuppressLint("SetJavaScriptEnabled")
@Composable
public actual fun rememberWebViewController(url: String): WebViewController<*> {
    val ctx = LocalContext.current

    val controller = remember {
        val wv = WebView(ctx)
        wv.settings.javaScriptEnabled = true
        wv.settings.domStorageEnabled = true
        wv.settings.javaScriptCanOpenWindowsAutomatically = true
        wv.settings.loadsImagesAutomatically = true
        AndroidWebViewController(AutoClosableWebView(wv))
    }

    LaunchedEffect(Unit) {
        controller.url = url
        controller.loadingState = LoadingState.Ready
    }

    return controller
}

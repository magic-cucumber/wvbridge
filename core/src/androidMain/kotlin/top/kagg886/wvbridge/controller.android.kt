package top.kagg886.wvbridge

import android.annotation.SuppressLint
import android.os.Handler
import android.os.Looper
import android.webkit.WebView
import androidx.compose.runtime.*
import androidx.compose.ui.platform.LocalContext
import androidx.webkit.Profile
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import kotlinx.coroutines.suspendCancellableCoroutine
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.bridge.WebMessageConsumer
import top.kagg886.wvbridge.config.WebViewConfig
import top.kagg886.wvbridge.interceptor.Interceptor
import top.kagg886.wvbridge.interceptor.InterceptorHandler
import top.kagg886.wvbridge.util.CloseHandle
import top.kagg886.wvbridge.util.LoggerReceiver
import java.util.concurrent.CopyOnWriteArraySet
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

private const val REMEMBER_TAG = "RememberWebViewController"

@JvmInline
internal value class AutoClosableWebView(public val instance: WebView) : AutoCloseable {
    override fun close() {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "close")
        instance.destroy()
    }

    internal companion object {
        private const val TAG = "AutoClosableWebView"
    }
}

internal class AndroidWebViewController(delegate: AutoClosableWebView, profile: Profile?) :
    WebViewController<AutoClosableWebView>(delegate) {
    internal val _interceptor by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "interceptor: lazy init")
        AndroidNavigationInterceptor()
    }

    internal val _navigator by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "navigator: lazy init")
        AndroidNavigator(delegate.instance)
    }
    internal val _bridge by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "bridge: lazy init")
        AndroidJavaScriptBridge(delegate.instance)
    }
    override val navigator: Navigator get() = _navigator
    override val bridge: JavaScriptBridge get() = _bridge
    override val interceptor: Interceptor get() = _interceptor

    internal companion object {
        private const val TAG = "AndroidWebViewController"
    }
}

internal class AndroidNavigationInterceptor : Interceptor {
    private val handlers: MutableMap<Int, MutableSet<InterceptorHandler>> = linkedMapOf()

    override fun registerNavigationInterceptor(
        index: Int,
        handler: InterceptorHandler,
    ): CloseHandle {
        val priorityHandlers = handlers.getOrPut(index) { mutableSetOf() }
        check(priorityHandlers.add(handler)) {
            "Navigation interceptor: [$handler] already registered at index=$index"
        }

        return object : CloseHandle {
            private var closed = false

            override fun close() {
                if (closed) return
                closed = true
                handlers[index]?.run {
                    remove(handler)
                    if (isEmpty()) handlers.remove(index)
                }
            }
        }
    }

    internal fun handleNavigation(url: String): InterceptorHandler.Result {
        handlers.keys.sorted().forEach { index ->
            handlers[index].orEmpty().forEach { handler ->
                when (val result = handler.handle(url)) {
                    InterceptorHandler.Result.Ignore -> Unit
                    else -> return result
                }
            }
        }
        return InterceptorHandler.Result.Allowed
    }
}

internal class AndroidJavaScriptBridge(private val instance: WebView) : JavaScriptBridge {
    private val mainHandler = Handler(Looper.getMainLooper())
    private val webMessageHandlers = CopyOnWriteArraySet<WebMessageConsumer>()

    private val supportWebMessageListener = WebViewFeature.isFeatureSupported(WebViewFeature.WEB_MESSAGE_LISTENER)

    init {
        onMainThread {
            if (!supportWebMessageListener) {
                LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "init: WEB_MESSAGE_LISTENER not supported")
                return@onMainThread
            }

            WebViewCompat.addWebMessageListener(
                instance,
                "_wvbridge",
                setOf("*"),
            ) { _, message, _, _, _ ->
                val data = message.data ?: ""
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "init: received message=$data")
                webMessageHandlers.forEach { it.consume(data) }
            }
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "init: native object registered")
        }
    }

    override suspend fun evaluateScript(script: String): String? {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "evaluateScript: script=$script")
        return suspendCancellableCoroutine { continuation ->
            onMainThread {
                runCatching {
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: on main thread, evaluating")
                    instance.evaluateJavascript(script) { result ->
                        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: result=$result")
                        continuation.resume(result)
                    }
                }.onFailure {
                    continuation.resumeWithException(it)
                }
            }
        }
    }

    override suspend fun registerDocumentStartHook(script: String): CloseHandle {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerDocumentStartHook: script=$script")
        return suspendCancellableCoroutine { continuation ->
            onMainThread {
                runCatching {
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook: on main thread")
                    if (!WebViewFeature.isFeatureSupported(WebViewFeature.DOCUMENT_START_SCRIPT)) {
                        LoggerReceiver.log(
                            LoggerReceiver.Level.ERROR,
                            TAG,
                            "registerDocumentStartHook: DOCUMENT_START_SCRIPT not supported"
                        )
                        throw UnsupportedOperationException("Android WebView document-start script injection is not supported")
                    }

                    val handler = WebViewCompat.addDocumentStartJavaScript(instance, script, setOf("*"))
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook: handler=$handler")
                    object : CloseHandle {
                        private var closed = false

                        override fun close() {
                            LoggerReceiver.log(
                                LoggerReceiver.Level.INFO,
                                TAG,
                                "registerDocumentStartHook.close: closed=$closed"
                            )
                            if (closed) {
                                LoggerReceiver.log(
                                    LoggerReceiver.Level.WARN,
                                    TAG,
                                    "registerDocumentStartHook.close: already closed, returning"
                                )
                                return
                            }
                            closed = true
                            LoggerReceiver.log(
                                LoggerReceiver.Level.VERBOSE,
                                TAG,
                                "registerDocumentStartHook.close: removing handler"
                            )
                            onMainThread {
                                handler.remove()
                                LoggerReceiver.log(
                                    LoggerReceiver.Level.VERBOSE,
                                    TAG,
                                    "registerDocumentStartHook.close: handler removed"
                                )
                            }
                        }
                    }
                }.onSuccess {
                    continuation.resume(it)
                }.onFailure {
                    continuation.resumeWithException(it)
                }
            }
        }
    }

    override suspend fun registerWebMessageHandler(handler: WebMessageConsumer): CloseHandle {
        if (!supportWebMessageListener) {
            LoggerReceiver.log(
                LoggerReceiver.Level.ERROR,
                TAG,
                "registerWebMessageHandler: WEB_MESSAGE_LISTENER not supported"
            )
            throw UnsupportedOperationException("Android WebView web-message listener is not supported")
        }
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerWebMessageHandler: handler=$handler")
        return suspendCancellableCoroutine { continuation ->
            onMainThread {
                runCatching {
                    check(webMessageHandlers.add(handler)) {
                        "Web message handler: [$handler] already added"
                    }
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler: handler added")

                    object : CloseHandle {
                        private var closed = false

                        override fun close() {
                            LoggerReceiver.log(
                                LoggerReceiver.Level.INFO,
                                TAG,
                                "registerWebMessageHandler.close: closed=$closed"
                            )
                            if (closed) {
                                LoggerReceiver.log(
                                    LoggerReceiver.Level.WARN,
                                    TAG,
                                    "registerWebMessageHandler.close: already closed, returning"
                                )
                                return
                            }
                            closed = true
                            LoggerReceiver.log(
                                LoggerReceiver.Level.VERBOSE,
                                TAG,
                                "registerWebMessageHandler.close: removing handler"
                            )
                            onMainThread {
                                webMessageHandlers.remove(handler)
                                LoggerReceiver.log(
                                    LoggerReceiver.Level.VERBOSE,
                                    TAG,
                                    "registerWebMessageHandler.close: handler removed"
                                )
                            }
                        }
                    }
                }.onSuccess {
                    continuation.resume(it)
                }.onFailure {
                    continuation.resumeWithException(it)
                }
            }
        }
    }

    private fun onMainThread(block: () -> Unit) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onMainThread")
        if (Looper.myLooper() == Looper.getMainLooper()) {
            LoggerReceiver.log(
                LoggerReceiver.Level.VERBOSE,
                TAG,
                "onMainThread: already on main thread, executing directly"
            )
            block()
        } else {
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "onMainThread: posting to main thread")
            mainHandler.post(block)
        }
    }

    internal companion object {
        private const val TAG = "AndroidJavaScriptBridge"
    }
}

internal class AndroidNavigator(private val instance: WebView) : Navigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goBack")
        instance.goBack()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goBack: instance.goBack() called")
        return instance.canGoBack()
    }

    override fun goForward(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goForward")
        instance.goForward()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goForward: instance.goForward() called")
        return instance.canGoForward()
    }

    override fun refresh(): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "refresh")
        instance.reload()
    }

    override fun stop(): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "stop")
        instance.stopLoading()
    }

    override fun loadUrl(url: String): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "loadUrl: url=$url")
        instance.loadUrl(url)
    }

    internal companion object {
        private const val TAG = "AndroidWebViewNavigator"
    }
}

@SuppressLint("SetJavaScriptEnabled")
@Composable
public actual fun rememberWebViewController(url: String, config: WebViewConfig): WebViewController<*> {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, REMEMBER_TAG, "rememberWebViewController: url=$url")
    val ctx = LocalContext.current
    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, REMEMBER_TAG, "rememberWebViewController: ctx=$ctx")

    val controller = remember(config.userAgent) {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, REMEMBER_TAG, "rememberWebViewController: creating WebView")
        val wv = WebView(ctx)
        wv.settings.javaScriptEnabled = true
        wv.settings.domStorageEnabled = true
        wv.settings.javaScriptCanOpenWindowsAutomatically = true
        wv.settings.loadsImagesAutomatically = true
        wv.settings.userAgentString = config.userAgent

        if (!WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE) && config.platform.profile != null) {
            throw UnsupportedOperationException("multi profile is not supported")
        }

        if (config.platform.profile != null) {
            WebViewCompat.setProfile(wv, config.platform.profile.name)
        }

        AndroidWebViewController(AutoClosableWebView(wv), config.platform.profile)
    }

    LaunchedEffect(Unit) {
        LoggerReceiver.log(
            LoggerReceiver.Level.INFO,
            REMEMBER_TAG,
            "rememberWebViewController: LaunchedEffect url=$url"
        )
        controller.url = url
        controller.loadingState = LoadingState.Ready
        LoggerReceiver.log(
            LoggerReceiver.Level.VERBOSE,
            REMEMBER_TAG,
            "rememberWebViewController: set loadingState=Ready"
        )
    }

    return controller
}

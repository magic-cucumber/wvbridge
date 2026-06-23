package top.kagg886.wvbridge

import androidx.compose.runtime.*
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.internal.WebViewBridgePanel

internal class SwingPanelController internal constructor(instance: WebViewBridgePanel) :
    WebViewController<WebViewBridgePanel>(instance) {
    internal val _bridge by lazy {
        SwingPanelJavaScriptBridge(instance)
    }
    internal val _navigator by lazy {
        SwingPanelNavigator(instance)
    }
    override val navigator: WebViewNavigator
        get() = _navigator
    override val bridge: JavaScriptBridge
        get() = _bridge
}

internal class SwingPanelJavaScriptBridge(private val instance: WebViewBridgePanel) : JavaScriptBridge {
    override suspend fun evaluateScript(script: String): JavaScriptBridge.Value =
        instance.evaluateScript(script).toJavaScriptValue()

    override suspend fun registerDocumentStartHook(script: String): CloseHandle {
        val hookId = instance.registerDocumentStartHook(script)
        return object : CloseHandle {
            private var closed = false

            override fun close() {
                if (closed) return
                closed = true
                instance.unregisterDocumentStartHook(hookId)
            }
        }
    }

    private fun String?.toJavaScriptValue(): JavaScriptBridge.Value = when (this) {
        null, "null" -> JavaScriptBridge.Value.Null
        "undefined" -> JavaScriptBridge.Value.Undefined
        else -> JavaScriptBridge.Value.ScriptObject(this)
    }
}

internal class SwingPanelNavigator(private val instance: WebViewBridgePanel) : WebViewNavigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean = instance.goBack()

    override fun goForward(): Boolean = instance.goForward()

    override fun refresh(): Unit = instance.refresh()
    override fun stop(): Unit = instance.stop()

    override fun loadUrl(url: String): Unit = instance.loadUrl(url)
}

@Composable
public actual fun rememberWebViewController(url: String): WebViewController<*> {
    var initialized by remember { mutableStateOf(false) }

    val controller = remember {
        SwingPanelController(instance = WebViewBridgePanel { initialized = true })
    }

    LaunchedEffect(initialized) {
        if (!initialized) {
            return@LaunchedEffect
        }
        controller.url = url
        controller.loadingState = LoadingState.Ready
    }

    return controller
}

package top.kagg886.wvbridge

import androidx.compose.runtime.*
import top.kagg886.wvbridge.internal.WebViewBridgePanel
import java.util.function.Consumer

public class SwingPanelState internal constructor(instance: WebViewBridgePanel) :
    WebViewState<WebViewBridgePanel>(instance) {
    internal val _navigator by lazy {
        SwingPanelNavigator(instance)
    }
    override val navigator: WebViewNavigator
        get() = _navigator
}

public class SwingPanelNavigator(private val instance: WebViewBridgePanel) : WebViewNavigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean = instance.goBack()

    override fun goForward(url: String): Boolean = instance.goForward()

    override fun refresh(): Unit = instance.refresh()

    override fun loadUrl(url: String): Unit = instance.loadUrl(url)
}

@Composable
public actual fun rememberWebViewState(url: String): WebViewState<*> {
    var initialized by remember { mutableStateOf(false) }

    val state = remember {
        SwingPanelState(instance = WebViewBridgePanel { initialized = true })
    }

    LaunchedEffect(initialized) {
        if (!initialized) {
            return@LaunchedEffect
        }
        state.state = LoadingState.Ready
    }

    return state
}

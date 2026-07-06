package top.kagg886.wvbridge

import androidx.compose.runtime.*
import kotlinx.coroutines.suspendCancellableCoroutine
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.bridge.WebMessageConsumer
import top.kagg886.wvbridge.internal.WebViewBridgePanel
import top.kagg886.wvbridge.util.LoggerReceiver
import javax.swing.SwingUtilities
import kotlin.coroutines.resume

internal class SwingPanelController internal constructor(instance: WebViewBridgePanel) :
    WebViewController<WebViewBridgePanel>(instance) {
    internal val _navigator by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "navigator: lazy init")
        SwingPanelNavigator(instance)
    }
    internal val _bridge by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "bridge: lazy init")
        SwingPanelJavaScriptBridge(instance)
    }
    override val navigator: Navigator get() = _navigator
    override val bridge: JavaScriptBridge get() = _bridge

    private companion object {
        private const val TAG = "SwingPanelCtrl"
    }
}

internal class SwingPanelJavaScriptBridge(private val instance: WebViewBridgePanel) : JavaScriptBridge {
    override suspend fun evaluateScript(script: String): String? {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "evaluateScript: script=$script")
        return suspendCancellableCoroutine { c ->
            SwingUtilities.invokeLater {
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: on EDT, evaluating")
                val result = instance.evaluateScript(script)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: result=$result")
                c.resume(result)
            }
        }
    }

    override suspend fun registerDocumentStartHook(script: String): CloseHandle {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerDocumentStartHook: script=$script")
        return suspendCancellableCoroutine {
            SwingUtilities.invokeLater {
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook: on EDT")
                val hookId = instance.registerDocumentStartHook(script)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook: hookId=$hookId")
                it.resume(object : CloseHandle {
                    private var closed = false

                    override fun close() {
                        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerDocumentStartHook.close: closed=$closed")
                        if (closed) {
                            LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerDocumentStartHook.close: already closed, returning")
                            return
                        }
                        if (instance.handle == 0L) {
                            closed = true
                            LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerDocumentStartHook.close: webview handle is null, returning")
                            return
                        }
                        closed = true
                        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook.close: unregistering hookId=$hookId")
                        instance.unregisterDocumentStartHook(hookId)
                        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook.close: hook unregistered")
                    }
                })
            }
        }
    }

    override suspend fun registerWebMessageHandler(handler: WebMessageConsumer): CloseHandle {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerWebMessageHandler: handler=$handler")
        return suspendCancellableCoroutine {
            SwingUtilities.invokeLater {
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler: on EDT")
                val handlerId = instance.registerWebMessageHandler(handler)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler: handlerId=$handlerId")
                it.resume(object : CloseHandle {
                    private var closed = false

                    override fun close() {
                        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerWebMessageHandler.close: closed=$closed")
                        if (closed) {
                            LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerWebMessageHandler.close: already closed, returning")
                            return
                        }
                        if (instance.handle == 0L) {
                            closed = true
                            LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerWebMessageHandler.close: webview handle is null, returning")
                            return
                        }
                        closed = true
                        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler.close: unregistering handlerId=$handlerId")
                        instance.unregisterWebMessageHandler(handlerId)
                        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler.close: handler unregistered")
                    }
                })
            }
        }
    }

    private companion object {
        private const val TAG = "SwingPanelJS"
    }
}

internal class SwingPanelNavigator(private val instance: WebViewBridgePanel) : Navigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goBack")
        val result = instance.goBack()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goBack: result=$result")
        return result
    }

    override fun goForward(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goForward")
        val result = instance.goForward()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goForward: result=$result")
        return result
    }

    override fun refresh(): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "refresh")
        instance.refresh()
    }

    override fun stop(): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "stop")
        instance.stop()
    }

    override fun loadUrl(url: String): Unit {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "loadUrl: url=$url")
        instance.loadUrl(url)
    }

    private companion object {
        private const val TAG = "SwingPanelNav"
    }
}

private const val TAG_RWVC = "RememberWVCtrl"

@Composable
public actual fun rememberWebViewController(url: String): WebViewController<*> {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG_RWVC, "rememberWebViewController: url=$url")
    var initialized by remember { mutableStateOf(false) }

    val controller = remember {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG_RWVC, "rememberWebViewController: creating WebViewBridgePanel")
        SwingPanelController(instance = WebViewBridgePanel { initialized = true })
    }

    LaunchedEffect(initialized) {
        if (!initialized) {
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG_RWVC, "rememberWebViewController: not yet initialized, waiting")
            return@LaunchedEffect
        }
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG_RWVC, "rememberWebViewController: initialized, setting url=$url")
        controller.url = url
        controller.loadingState = LoadingState.Ready
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG_RWVC, "rememberWebViewController: set loadingState=Ready")
    }

    return controller
}

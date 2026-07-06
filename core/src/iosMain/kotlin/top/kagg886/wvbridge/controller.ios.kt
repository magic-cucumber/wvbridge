package top.kagg886.wvbridge

import androidx.compose.runtime.*
import kotlinx.coroutines.suspendCancellableCoroutine
import platform.Foundation.NSError
import platform.Foundation.NSURL
import platform.Foundation.NSURLRequest
import platform.WebKit.WKScriptMessage
import platform.WebKit.WKScriptMessageHandlerProtocol
import platform.WebKit.WKUserContentController
import platform.WebKit.WKUserScript
import platform.WebKit.WKUserScriptInjectionTime
import platform.WebKit.WKWebView
import platform.darwin.NSObject
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.bridge.JavaScriptBridge
import top.kagg886.wvbridge.bridge.WebMessageConsumer
import top.kagg886.wvbridge.util.LoggerReceiver
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

internal class AutoClosableWKWebView(public val delegate: WKWebView) : AutoCloseable {
    override fun close() {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "close")
    }
    private companion object {
        private const val TAG = "AutoCloseWKWV"
    }
}

internal class WKWebViewController(instance: AutoClosableWKWebView) : WebViewController<AutoClosableWKWebView>(instance) {
    internal val _navigator by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "creating WKWebViewNavigator")
        WKNavigator(instance.delegate)
    }
    internal val _bridge by lazy {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "creating WKJavaScriptBridge")
        WKJavaScriptBridge(instance.delegate)
    }
    override val navigator: Navigator get() = _navigator
    override val bridge: JavaScriptBridge get() = _bridge

    private companion object {
        private const val TAG = "WKVWCtrl"
    }
}

internal class WKJavaScriptBridge(private val instance: WKWebView) : JavaScriptBridge {
    private var nextDocumentStartHookId = 0L
    private val documentStartHooks = linkedMapOf<Long, String>()
    private val webMessageHandlers = linkedSetOf<WebMessageConsumer>()
    private val webMessageDispatcher = WKWebMessageDispatcher(webMessageHandlers)

    init {
        instance.configuration.userContentController.addScriptMessageHandler(webMessageDispatcher, name = "wvbridge")
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "init: native handler registered")
    }

    override suspend fun evaluateScript(script: String): String? {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "evaluateScript: script=$script")
        return suspendCancellableCoroutine { continuation ->
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: calling evaluateJavaScript")
            instance.evaluateJavaScript(script) { result, error ->
                if (error != null) {
                    LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "evaluateScript: error=${error.localizedDescription}")
                    continuation.resumeWithException(error.toException())
                } else {
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "evaluateScript: success, result=${result?.toString()}")
                    continuation.resume(result?.toString())
                }
            }
        }
    }

    override suspend fun registerDocumentStartHook(script: String): CloseHandle {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerDocumentStartHook: script=$script")
        val id = nextDocumentStartHookId++
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook: assigned id=$id")
        documentStartHooks[id] = script
        addDocumentStartUserScript(script)

        return object : CloseHandle {
            private var closed = false

            override fun close() {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerDocumentStartHook.close: id=$id")
                if (closed) {
                    LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerDocumentStartHook.close: id=$id, already closed")
                    return
                }
                closed = true
                documentStartHooks.remove(id)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerDocumentStartHook.close: removed id=$id, triggering rebuild")
                rebuildDocumentStartUserScripts()
            }
        }
    }

    override suspend fun registerWebMessageHandler(handler: WebMessageConsumer): CloseHandle {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerWebMessageHandler: handler=$handler")
        check(webMessageHandlers.add(handler)) {
            "Web message handler: [$handler] already added"
        }
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler: handler added")

        return object : CloseHandle {
            private var closed = false

            override fun close() {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "registerWebMessageHandler.close: closed=$closed")
                if (closed) {
                    LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "registerWebMessageHandler.close: already closed, returning")
                    return
                }
                closed = true
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler.close: removing handler")
                webMessageHandlers.remove(handler)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "registerWebMessageHandler.close: handler removed")
            }
        }
    }

    private fun rebuildDocumentStartUserScripts() {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "rebuildDocumentStartUserScripts: count=${documentStartHooks.size}")
        instance.configuration.userContentController.removeAllUserScripts()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "rebuildDocumentStartUserScripts: user scripts removed, re-adding hooks")
        documentStartHooks.values.forEach(::addDocumentStartUserScript)
    }

    private fun addDocumentStartUserScript(script: String) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "addDocumentStartUserScript: script=$script")
        val userScript = WKUserScript(
            source = script,
            injectionTime = WKUserScriptInjectionTime.WKUserScriptInjectionTimeAtDocumentStart,
            forMainFrameOnly = false,
        )
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "addDocumentStartUserScript: WKUserScript created, adding to userContentController")
        instance.configuration.userContentController.addUserScript(userScript)
    }

    private fun NSError.toException(): RuntimeException {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "toException: description=$localizedDescription")
        return RuntimeException("WKWebView JavaScript evaluation failed: $localizedDescription")
    }

    private companion object {
        private const val TAG = "WKJSBridge"
    }
}

private class WKWebMessageDispatcher(
    private val handlers: Set<WebMessageConsumer>,
) : NSObject(), WKScriptMessageHandlerProtocol {
    private val TAG = "WKMsgDispatcher"
    override fun userContentController(
        userContentController: WKUserContentController,
        didReceiveScriptMessage: WKScriptMessage,
    ) {
        val body = didReceiveScriptMessage.body
        val message = body as? String ?: body.toString()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "userContentController: received message=$message")
        handlers.forEach { it.consume(message) }
    }
}

internal class WKNavigator(private val instance: WKWebView) : Navigator {
    override var canGoBack: Boolean by mutableStateOf(false)
        internal set
    override var canGoForward: Boolean by mutableStateOf(false)
        internal set

    override fun goBack(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goBack")
        instance.goBack()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goBack: after goBack, canGoBack=${instance.canGoBack()}")
        return instance.canGoBack()
    }

    override fun goForward(): Boolean {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "goForward")
        instance.goForward()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "goForward: after goForward, canGoForward=${instance.canGoForward()}")
        return instance.canGoForward()
    }

    override fun refresh() {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "refresh")
        instance.reload()
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "refresh: reload called")
    }

    override fun stop() {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "stop")
        instance.stopLoading()
    }

    override fun loadUrl(url: String) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "loadUrl: url=$url")
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "loadUrl: creating NSURLRequest")
        instance.loadRequest(NSURLRequest.requestWithURL(NSURL.URLWithString(url)!!))
    }

    private companion object {
        private const val TAG = "WKWebViewNav"
    }
}

private val TAG_RWVC = "RememberWVCtrl"

@Composable
public actual fun rememberWebViewController(url: String): WebViewController<*> {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG_RWVC, "rememberWebViewController: url=$url")
    val controller = remember {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG_RWVC, "rememberWebViewController: creating WKWebView")
        val wv = WKWebView()
        wv.configuration.defaultWebpagePreferences.allowsContentJavaScript = true
        wv.configuration.preferences.javaScriptCanOpenWindowsAutomatically = true

        WKWebViewController(AutoClosableWKWebView(wv))
    }

    LaunchedEffect(Unit) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG_RWVC, "rememberWebViewController: LaunchedEffect launching, setting url=$url")
        controller.url = url
        controller.loadingState = LoadingState.Ready
    }

    return controller
}

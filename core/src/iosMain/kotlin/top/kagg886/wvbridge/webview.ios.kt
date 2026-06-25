package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.UIKitView
import kotlinx.cinterop.COpaquePointer
import kotlinx.cinterop.ExperimentalForeignApi
import kotlinx.cinterop.ObjCSignatureOverride
import platform.Foundation.NSError
import platform.Foundation.NSKeyValueChangeNewKey
import platform.Foundation.NSKeyValueObservingOptionNew
import platform.Foundation.addObserver
import platform.Foundation.removeObserver
import platform.WebKit.WKNavigation
import platform.WebKit.WKNavigationDelegateProtocol
import platform.WebKit.WKWebView
import platform.darwin.NSObject
import top.kagg886.wvbridge.internal.WVBKVOObserverProtocolProtocol
import top.kagg886.wvbridge.util.LoggerReceiver

private val TAG = "WebViewIOS"

private fun NSError.toLoadingReason(): String {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "toLoadingReason: domain=$domain, code=$code, message=$localizedDescription")
    return "wkwebview.navigation.failed: domain=$domain, code=$code, message=$localizedDescription"
}

@OptIn(ExperimentalForeignApi::class)
@Composable
public actual fun WebView(controller: WebViewController<*>, modifier: Modifier) {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: url=${controller.url}")
    controller as WKWebViewController
    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: cast to WKWebViewController OK")

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: setting up WKNavigationDelegate")
        val webView = controller.instance.delegate
        val delegate = object : NSObject(), WKNavigationDelegateProtocol {
            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didStartProvisionalNavigation: WKNavigation?) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WKNavigationDelegate: didStartProvisionalNavigation")
                controller.loadingState = LoadingState.Loading(0f)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFinishNavigation: WKNavigation?) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WKNavigationDelegate: didFinishNavigation")
                controller.loadingState = LoadingState.LoadingEnd(true, null)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFailNavigation: WKNavigation?, withError: NSError) {
                LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "WKNavigationDelegate: didFailNavigation: error=${withError.toLoadingReason()}")
                controller.loadingState = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }

            @ObjCSignatureOverride
            override fun webView(
                webView: WKWebView,
                didFailProvisionalNavigation: WKNavigation?,
                withError: NSError
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "WKNavigationDelegate: didFailProvisionalNavigation: error=${withError.toLoadingReason()}")
                controller.loadingState = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }
        }

        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: WKNavigationDelegate created, assigning to webView")
        webView.navigationDelegate = delegate
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: WKNavigationDelegate disposed")
            webView.navigationDelegate = null
        }
    }

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: setting up KVO observer for estimatedProgress")
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: estimatedProgress changed: keyPath=$keyPath, newValue=${change?.get(NSKeyValueChangeNewKey)}")
                val progress = (change?.get(NSKeyValueChangeNewKey) as? Number)?.toFloat() ?: 0f
                controller.loadingState = LoadingState.Loading(progress.coerceIn(0f, 1f))
            }
        }

        webView.addObserver(observer, "estimatedProgress", NSKeyValueObservingOptionNew, null)
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: KVO estimatedProgress observer added")
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: KVO estimatedProgress disposed")
            webView.removeObserver(observer, "estimatedProgress")
        }
    }

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: setting up KVO observer for URL")
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: URL changed: keyPath=$keyPath, url=${webView.URL?.absoluteString}")
                controller.url = webView.URL?.absoluteString ?: ""
            }
        }

        webView.addObserver(observer, "URL", NSKeyValueObservingOptionNew, null)
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: KVO URL observer added")
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: KVO URL disposed")
            webView.removeObserver(observer, "URL")
        }
    }

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: setting up KVO observer for canGoBack")
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: canGoBack changed: keyPath=$keyPath, value=${webView.canGoBack()}")
                controller._navigator.canGoBack = webView.canGoBack()
            }
        }

        webView.addObserver(observer, "canGoBack", NSKeyValueObservingOptionNew, null)
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: KVO canGoBack observer added")
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: KVO canGoBack disposed")
            webView.removeObserver(observer, "canGoBack")
        }
    }

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: setting up KVO observer for canGoForward")
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: canGoForward changed: keyPath=$keyPath, value=${webView.canGoForward()}")
                controller._navigator.canGoForward = webView.canGoForward()
            }
        }

        webView.addObserver(observer, "canGoForward", NSKeyValueObservingOptionNew, null)
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: KVO canGoForward observer added")
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: KVO canGoForward disposed")
            webView.removeObserver(observer, "canGoForward")
        }
    }

    LaunchedEffect(controller.loadingState) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: LaunchedEffect triggered: loadingState=${controller.loadingState}")
        if (controller.loadingState == LoadingState.Ready) {
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: loadingState is Ready, loading url=${controller.url}")
            controller.navigator.loadUrl(controller.url)
        }
    }

    UIKitView(
        factory = { controller.instance.delegate },
        modifier = modifier,
    )
}

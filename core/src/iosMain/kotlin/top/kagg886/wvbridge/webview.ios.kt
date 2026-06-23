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

private fun NSError.toLoadingReason(): String =
    "wkwebview.navigation.failed: domain=$domain, code=$code, message=$localizedDescription"

@OptIn(ExperimentalForeignApi::class)
@Composable
public actual fun WebView(controller: WebViewController<*>, modifier: Modifier) {
    controller as WKWebViewController

    DisposableEffect(controller) {
        val webView = controller.instance.delegate
        val delegate = object : NSObject(), WKNavigationDelegateProtocol {
            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didStartProvisionalNavigation: WKNavigation?) {
                controller.loadingState = LoadingState.Loading(0f)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFinishNavigation: WKNavigation?) {
                controller.loadingState = LoadingState.LoadingEnd(true, null)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFailNavigation: WKNavigation?, withError: NSError) {
                controller.loadingState = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }

            @ObjCSignatureOverride
            override fun webView(
                webView: WKWebView,
                didFailProvisionalNavigation: WKNavigation?,
                withError: NSError
            ) {
                controller.loadingState = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }
        }

        webView.navigationDelegate = delegate
        onDispose {
            webView.navigationDelegate = null
        }
    }

    DisposableEffect(controller) {
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                val progress = (change?.get(NSKeyValueChangeNewKey) as? Number)?.toFloat() ?: 0f
                controller.loadingState = LoadingState.Loading(progress.coerceIn(0f, 1f))
            }
        }

        webView.addObserver(observer, "estimatedProgress", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "estimatedProgress")
        }
    }

    DisposableEffect(controller) {
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                controller.url = webView.URL?.absoluteString ?: ""
            }
        }

        webView.addObserver(observer, "URL", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "URL")
        }
    }

    DisposableEffect(controller) {
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                controller._navigator.canGoBack = webView.canGoBack()
            }
        }

        webView.addObserver(observer, "canGoBack", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "canGoBack")
        }
    }

    DisposableEffect(controller) {
        val webView = controller.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                controller._navigator.canGoForward = webView.canGoForward()
            }
        }

        webView.addObserver(observer, "canGoForward", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "canGoForward")
        }
    }

    LaunchedEffect(controller.loadingState) {
        if (controller.loadingState == LoadingState.Ready) {
            controller.navigator.loadUrl(controller.url)
        }
    }

    UIKitView(
        factory = { controller.instance.delegate },
        modifier = modifier,
    )
}

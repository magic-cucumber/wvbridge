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
public actual fun WebView(state: WebViewState<*>, modifier: Modifier) {
    state as WKWebViewState

    DisposableEffect(state) {
        val webView = state.instance.delegate
        val delegate = object : NSObject(), WKNavigationDelegateProtocol {
            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didStartProvisionalNavigation: WKNavigation?) {
                state.state = LoadingState.Loading(0f)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFinishNavigation: WKNavigation?) {
                state.state = LoadingState.LoadingEnd(true, null)
            }

            @ObjCSignatureOverride
            override fun webView(webView: WKWebView, didFailNavigation: WKNavigation?, withError: NSError) {
                state.state = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }

            @ObjCSignatureOverride
            override fun webView(
                webView: WKWebView,
                didFailProvisionalNavigation: WKNavigation?,
                withError: NSError
            ) {
                state.state = LoadingState.LoadingEnd(false, withError.toLoadingReason())
            }
        }

        webView.navigationDelegate = delegate
        onDispose {
            webView.navigationDelegate = null
        }
    }

    DisposableEffect(state) {
        val webView = state.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                val progress = (change?.get(NSKeyValueChangeNewKey) as? Number)?.toFloat() ?: 0f
                state.state = LoadingState.Loading(progress.coerceIn(0f, 1f))
            }
        }

        webView.addObserver(observer, "estimatedProgress", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "estimatedProgress")
        }
    }

    DisposableEffect(state) {
        val webView = state.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                state.url = webView.URL?.absoluteString ?: ""
            }
        }

        webView.addObserver(observer, "URL", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "URL")
        }
    }

    DisposableEffect(state) {
        val webView = state.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                state._navigator.canGoBack = webView.canGoBack()
            }
        }

        webView.addObserver(observer, "canGoBack", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "canGoBack")
        }
    }

    DisposableEffect(state) {
        val webView = state.instance.delegate
        val observer = object : WVBKVOObserverProtocolProtocol, NSObject() {
            override fun observeValueForKeyPath(
                keyPath: String?,
                ofObject: Any?,
                change: Map<Any?, *>?,
                context: COpaquePointer?
            ) {
                state._navigator.canGoForward = webView.canGoForward()
            }
        }

        webView.addObserver(observer, "canGoForward", NSKeyValueObservingOptionNew, null)
        onDispose {
            webView.removeObserver(observer, "canGoForward")
        }
    }

    LaunchedEffect(state.state) {
        if (state.state == LoadingState.Ready) {
            state.navigator.loadUrl(state.url)
        }
    }

    UIKitView(
        factory = { state.instance.delegate },
        modifier = modifier,
    )
}

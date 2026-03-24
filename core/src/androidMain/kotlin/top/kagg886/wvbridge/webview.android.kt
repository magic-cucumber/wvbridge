package top.kagg886.wvbridge

import android.graphics.Bitmap
import android.webkit.WebChromeClient
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView

@Composable
public actual fun WebView(state: WebViewState<*>, modifier: Modifier) {
    state as AndroidWebViewState

    DisposableEffect(state) {
        val webView = state.instance.instance
        webView.webChromeClient = object : WebChromeClient() {
            override fun onProgressChanged(view: WebView?, newProgress: Int) {
                super.onProgressChanged(view, newProgress)
                state.state = LoadingState.Loading(newProgress.coerceIn(0, 100) / 100f)
            }
        }
        onDispose {
            webView.webChromeClient = null
        }
    }

    DisposableEffect(state) {
        val webView = state.instance.instance
        webView.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
                super.onPageStarted(view, url, favicon)
                state.url = url ?: state.url
                state.state = LoadingState.Loading(0f)
            }

            override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
                super.doUpdateVisitedHistory(view, url, isReload)
                state.url = url ?: state.url
                state._navigator.canGoBack = webView.canGoBack()
                state._navigator.canGoForward = webView.canGoForward()
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
                state.url = url ?: state.url

                if (state.state !is LoadingState.LoadingEnd) {
                    state.state = LoadingState.LoadingEnd(true, null)
                }

                state._navigator.canGoBack = webView.canGoBack()
                state._navigator.canGoForward = webView.canGoForward()
            }

            override fun onReceivedError(
                view: WebView?,
                request: WebResourceRequest?,
                error: WebResourceError?,
            ) {
                super.onReceivedError(view, request, error)
                if (request?.isForMainFrame != false) {
                    state.state = LoadingState.LoadingEnd(
                        success = false,
                        reason = error?.description?.toString(),
                    )

                    state._navigator.canGoBack = webView.canGoBack()
                    state._navigator.canGoForward = webView.canGoForward()
                }
            }
        }

        onDispose {
            webView.webViewClient = WebViewClient()
        }
    }

    LaunchedEffect(state.state) {
        if (state.state == LoadingState.Ready) {
            state.navigator.loadUrl(state.url)
        }
    }

    AndroidView(
        factory = { state.instance.instance },
        modifier = modifier,
    )
}

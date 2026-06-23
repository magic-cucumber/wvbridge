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
public actual fun WebView(controller: WebViewController<*>, modifier: Modifier) {
    controller as AndroidWebViewController

    DisposableEffect(controller) {
        val webView = controller.instance.instance
        webView.webChromeClient = object : WebChromeClient() {
            override fun onProgressChanged(view: WebView?, newProgress: Int) {
                super.onProgressChanged(view, newProgress)
                controller.loadingState = LoadingState.Loading(newProgress.coerceIn(0, 100) / 100f)
            }
        }
        onDispose {
            webView.webChromeClient = null
        }
    }

    DisposableEffect(controller) {
        val webView = controller.instance.instance
        webView.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
                super.onPageStarted(view, url, favicon)
                controller.url = url ?: controller.url
                controller.loadingState = LoadingState.Loading(0f)
            }

            override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
                super.doUpdateVisitedHistory(view, url, isReload)
                controller.url = url ?: controller.url
                controller._navigator.canGoBack = webView.canGoBack()
                controller._navigator.canGoForward = webView.canGoForward()
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                super.onPageFinished(view, url)
                controller.url = url ?: controller.url

                if (controller.loadingState !is LoadingState.LoadingEnd) {
                    controller.loadingState = LoadingState.LoadingEnd(true, null)
                }

                controller._navigator.canGoBack = webView.canGoBack()
                controller._navigator.canGoForward = webView.canGoForward()
            }

            override fun onReceivedError(
                view: WebView?,
                request: WebResourceRequest?,
                error: WebResourceError?,
            ) {
                super.onReceivedError(view, request, error)
                if (request?.isForMainFrame != false) {
                    controller.loadingState = LoadingState.LoadingEnd(
                        success = false,
                        reason = error?.description?.toString(),
                    )

                    controller._navigator.canGoBack = webView.canGoBack()
                    controller._navigator.canGoForward = webView.canGoForward()
                }
            }
        }

        onDispose {
            webView.webViewClient = WebViewClient()
        }
    }

    LaunchedEffect(controller.loadingState) {
        if (controller.loadingState == LoadingState.Ready) {
            controller.navigator.loadUrl(controller.url)
        }
    }

    AndroidView(
        factory = { controller.instance.instance },
        modifier = modifier,
    )
}

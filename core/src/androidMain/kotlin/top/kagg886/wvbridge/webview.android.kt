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
import top.kagg886.wvbridge.util.LoggerReceiver

private const val TAG = "WebViewAndroid"

@Composable
public actual fun WebView(controller: WebViewController<*>, modifier: Modifier) {
    LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "WebView: controller=$controller")

    controller as AndroidWebViewController
    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "WebView: controller cast to AndroidWebViewController")

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "DisposableEffect: creating WebChromeClient")
        val webView = controller.instance.instance
        webView.webChromeClient = object : WebChromeClient() {
            override fun onProgressChanged(view: WebView?, newProgress: Int) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onProgressChanged: view=$view, newProgress=$newProgress")
                super.onProgressChanged(view, newProgress)
                val progress = newProgress.coerceIn(0, 100) / 100f
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "onProgressChanged: progress=$progress")
                controller.loadingState = LoadingState.Loading(progress)
            }
        }
        onDispose {
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "DisposableEffect: onDispose clearing WebChromeClient")
            webView.webChromeClient = null
        }
    }

    DisposableEffect(controller) {
        LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "DisposableEffect: creating WebViewClient")
        val webView = controller.instance.instance
        webView.webViewClient = object : WebViewClient() {
            override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onPageStarted: url=$url")
                super.onPageStarted(view, url, favicon)
                controller.url = url ?: controller.url
                controller.loadingState = LoadingState.Loading(0f)
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "onPageStarted: set loadingState=Loading(0f)")
            }

            override fun doUpdateVisitedHistory(view: WebView?, url: String?, isReload: Boolean) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "doUpdateVisitedHistory: url=$url, isReload=$isReload")
                super.doUpdateVisitedHistory(view, url, isReload)
                controller.url = url ?: controller.url
                controller._navigator.canGoBack = webView.canGoBack()
                controller._navigator.canGoForward = webView.canGoForward()
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "doUpdateVisitedHistory: canGoBack=${controller._navigator.canGoBack}, canGoForward=${controller._navigator.canGoForward}")
            }

            override fun onPageFinished(view: WebView?, url: String?) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onPageFinished: url=$url")
                super.onPageFinished(view, url)
                controller.url = url ?: controller.url

                if (controller.loadingState !is LoadingState.LoadingEnd) {
                    LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "onPageFinished: setting LoadingEnd(success=true)")
                    controller.loadingState = LoadingState.LoadingEnd(true, null)
                }

                controller._navigator.canGoBack = webView.canGoBack()
                controller._navigator.canGoForward = webView.canGoForward()
                LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "onPageFinished: canGoBack=${controller._navigator.canGoBack}, canGoForward=${controller._navigator.canGoForward}")
            }

            override fun onReceivedError(
                view: WebView?,
                request: WebResourceRequest?,
                error: WebResourceError?,
            ) {
                LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "onReceivedError: request=$request, error=$error")
                super.onReceivedError(view, request, error)
                if (request?.isForMainFrame != false) {
                    LoggerReceiver.log(LoggerReceiver.Level.WARN, TAG, "onReceivedError: main frame error, reason=${error?.description}")
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
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "DisposableEffect: onDispose resetting WebViewClient")
            webView.webViewClient = WebViewClient()
        }
    }

    LaunchedEffect(controller.loadingState) {
        LoggerReceiver.log(LoggerReceiver.Level.INFO, TAG, "LaunchedEffect: loadingState=${controller.loadingState}")
        if (controller.loadingState == LoadingState.Ready) {
            LoggerReceiver.log(LoggerReceiver.Level.VERBOSE, TAG, "LaunchedEffect: loadingState is Ready, calling loadUrl=${controller.url}")
            controller.navigator.loadUrl(controller.url)
        }
    }

    AndroidView(
        factory = { controller.instance.instance },
        modifier = modifier,
    )
}

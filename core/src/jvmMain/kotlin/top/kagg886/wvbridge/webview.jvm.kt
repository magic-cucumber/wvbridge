package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.awt.SwingPanel
import java.awt.Component
import java.util.function.BiConsumer
import java.util.function.Consumer
import javax.swing.SwingUtilities

@Composable
public actual fun WebView(controller: WebViewController<*>, modifier: Modifier) {
    controller as SwingPanelController

    DisposableEffect(Unit) {
        onDispose {
            controller.instance.close()
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<String> = {
            controller.loadingState = LoadingState.Loading(0.0f)
            controller.url = it
        }

        controller.instance.addPageLoadingStartListener(listener)
        onDispose {
            controller.instance.removePageLoadingStartListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<String?> = {
            if (it != null) {
                SwingUtilities.invokeLater {
                    if (controller.instance.isDisplayable && controller.instance.isShowing) {
                        controller.instance.addNotify()
                    }
                }
            }
        }

        controller.instance.addWebViewCloseListener(listener)
        onDispose {
            controller.instance.removeWebViewCloseListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<String> = {
            controller.url = it
        }

        controller.instance.addURLChangeListener(listener)
        onDispose {
            controller.instance.removeURLChangeListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Float> = {
            controller.loadingState = LoadingState.Loading(it)
        }

        controller.instance.addPageLoadingProgressListener(listener)
        onDispose {
            controller.instance.removePageLoadingProgressListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: BiConsumer<Boolean, String?> = { success,reason->
            controller.loadingState = LoadingState.LoadingEnd(success,reason)
        }
        controller.instance.addPageLoadingEndListener(listener)
        onDispose {
            controller.instance.removePageLoadingEndListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Boolean> = {
            controller._navigator.canGoBack = it
        }

        controller.instance.addCanGoBackChangeListener(listener)

        onDispose {
            controller.instance.removeCanGoBackChangeListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Boolean> = {
            controller._navigator.canGoForward = it
        }

        controller.instance.addCanGoForwardChangeListener(listener)

        onDispose {
            controller.instance.removeCanGoForwardChangeListener(listener)
        }
    }

    LaunchedEffect(controller.loadingState) {
        if (controller.loadingState == LoadingState.Ready) {
            controller.navigator.loadUrl(controller.url)
        }
    }

    SwingPanel(
        factory = { controller.instance },
        modifier = modifier,
    )
}

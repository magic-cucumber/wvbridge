package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.awt.SwingPanel
import java.awt.Component
import java.util.function.Consumer

@Composable
public actual fun WebView(state: WebViewState<*>, modifier: Modifier) {
    state as SwingPanelState

    DisposableEffect(Unit) {
        val listener: Consumer<String> = {
            state.url = it
            state.state = LoadingState.Loading(0.0f)
        }

        state.instance.addPageLoadingStartListener(listener)
        onDispose {
            state.instance.removePageLoadingStartListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Float> = {
            state.state = LoadingState.Loading(it)
        }

        state.instance.addPageLoadingProgressListener(listener)
        onDispose {
            state.instance.removePageLoadingProgressListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Boolean> = {
            state.state = LoadingState.LoadingEnd(it)
        }
        state.instance.addPageLoadingEndListener(listener)
        onDispose {
            state.instance.removePageLoadingEndListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Boolean> = {
            state._navigator.canGoBack = it
        }

        state.instance.addCanGoBackChangeListener(listener)

        onDispose {
            state.instance.removeCanGoBackChangeListener(listener)
        }
    }

    DisposableEffect(Unit) {
        val listener: Consumer<Boolean> = {
            state._navigator.canGoForward = it
        }

        state.instance.addCanGoForwardChangeListener(listener)

        onDispose {
            state.instance.removeCanGoForwardChangeListener(listener)
        }
    }

    LaunchedEffect(state.state) {
        if (state.state == LoadingState.Ready) {
            state.navigator.loadUrl(state.url)
        }
    }

    SwingPanel(
        factory = { state.instance },
        modifier = modifier,
    )
}

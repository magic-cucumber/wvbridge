package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/3/23 12:47
 * ================================================
 */

public abstract class WebViewState<T : AutoCloseable> internal constructor(internal val instance: T) :
    AutoCloseable by instance {
    public var url: String by mutableStateOf("")
        internal set

    public var state: LoadingState by mutableStateOf(LoadingState.NotReady)
        internal set

    public abstract val navigator: WebViewNavigator
}

@Composable
public expect fun rememberWebViewState(url: String = "about:blank"): WebViewState<*>

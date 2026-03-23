package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/3/23 12:47
 * ================================================
 */

@Composable
public expect fun WebView(
    state: WebViewState<*> = rememberWebViewState("about:blank"),
    modifier: Modifier = Modifier,
)

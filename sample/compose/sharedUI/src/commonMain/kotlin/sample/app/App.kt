package sample.app

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import top.kagg886.wvbridge.LoadingState
import top.kagg886.wvbridge.WebView
import top.kagg886.wvbridge.WebViewNavigator
import top.kagg886.wvbridge.rememberWebViewState

@Composable
fun App() {
    MaterialTheme {
        Surface(modifier = Modifier.fillMaxSize()) {
            Box(
                modifier = Modifier.fillMaxSize().padding(16.dp),
                contentAlignment = Alignment.Center,
            ) {
                BrowserDialog(initialUrl = "https://www.baidu.com")
            }
        }
    }
}

@Composable
private fun BrowserDialog(initialUrl: String) {
    val webViewState = rememberWebViewState(initialUrl)
    var urlInput by rememberSaveable { mutableStateOf(initialUrl) }

    val loadingState = webViewState.state
    val isLoading = loadingState is LoadingState.Loading
    val loadingProgress = when (loadingState) {
        is LoadingState.Loading -> loadingState.progress.coerceIn(0f, 1f)
        is LoadingState.LoadingEnd -> 1f
        else -> 0f
    }

    LaunchedEffect(webViewState.state) {
        val state = webViewState.state
        if (state is LoadingState.LoadingEnd && !state.success) {
            urlInput = "加载错误！"
        }
    }

    LaunchedEffect(webViewState.url) {
        if (webViewState.url.isNotBlank() && webViewState.url != urlInput) {
            urlInput = webViewState.url
        }
    }

    Column(modifier = Modifier.fillMaxSize().padding(12.dp)) {
        Text(
            text = "浏览器对话框",
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(horizontal = 4.dp, vertical = 6.dp),
        )
        if (isLoading) {
            LinearProgressIndicator(
                progress = { loadingProgress },
                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
            )
        }
        BrowserToolbar(
            urlInput = urlInput,
            onUrlInputChange = { urlInput = it },
            onSubmitUrl = { webViewState.navigator.loadUrl(urlInput) },
            navigator = webViewState.navigator,
            isLoading = isLoading,
        )
        WebView(
            state = webViewState,
            modifier = Modifier.fillMaxSize().padding(top = 10.dp),
        )
    }
}

@Composable
private fun BrowserToolbar(
    urlInput: String,
    onUrlInputChange: (String) -> Unit,
    onSubmitUrl: () -> Unit,
    navigator: WebViewNavigator,
    isLoading: Boolean,
) {
    Row(
        modifier = Modifier.padding(top = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        IconButton(
            onClick = { navigator.goBack() },
            enabled = navigator.canGoBack,
            modifier = Modifier.size(40.dp),
        ) {
            Text("←")
        }
        IconButton(
            onClick = { navigator.goForward(urlInput) },
            enabled = navigator.canGoForward,
            modifier = Modifier.size(40.dp),
        ) {
            Text("→")
        }
        IconButton(
            onClick = { navigator.refresh() },
            modifier = Modifier.size(40.dp),
        ) {
            Text("⟳")
        }
        IconButton(
            onClick = { navigator.stop() },
            enabled = isLoading,
            modifier = Modifier.size(40.dp),
        ) {
            Text("⏹")
        }
        OutlinedTextField(
            value = urlInput,
            onValueChange = onUrlInputChange,
            singleLine = true,
            modifier = Modifier.weight(1f),
            label = { Text("URL") },
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Go),
            keyboardActions = KeyboardActions(onGo = { onSubmitUrl() }),
        )
    }
}

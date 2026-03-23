package sample.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
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
    var errorMessage by remember { mutableStateOf<String?>(null) }

    val loadingState = webViewState.state
    val isLoading = loadingState is LoadingState.Loading
    val loadingProgress = when (loadingState) {
        is LoadingState.Loading -> loadingState.progress.coerceIn(0f, 1f)
        is LoadingState.LoadingEnd -> 1f
        else -> 0f
    }

    fun runNavigatorAction(fallbackError: String, action: () -> Any?) {
        val result = runCatching(action)
        result.exceptionOrNull()?.let {
            errorMessage = it.cause?.message ?: it.message ?: fallbackError
        }
    }

    fun loadCurrentInput() {
        val normalized = urlInput.trim()
        if (normalized.isBlank()) return
        runNavigatorAction("Navigation failed") {
            webViewState.navigator.loadUrl(normalized)
        }
    }

    LaunchedEffect(webViewState.url) {
        if (webViewState.url.isNotBlank() && webViewState.url != urlInput) {
            urlInput = webViewState.url
        }
    }
    LaunchedEffect(webViewState.state) {
        if (webViewState.state == LoadingState.Ready && webViewState.url.isBlank()) {
            webViewState.navigator.loadUrl(initialUrl)
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
            onSubmitUrl = ::loadCurrentInput,
            navigator = webViewState.navigator,
            isLoading = isLoading,
            onActionError = { errorMessage = it },
        )
        WebView(
            state = webViewState,
            modifier = Modifier.fillMaxSize().padding(top = 10.dp),
        )
    }


    if (errorMessage != null) {
        AlertDialog(
            onDismissRequest = { errorMessage = null },
            title = { Text("操作失败") },
            text = { Text(errorMessage ?: "") },
            confirmButton = {
                Button(onClick = { errorMessage = null }) {
                    Text("确定")
                }
            },
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
    onActionError: (String) -> Unit,
) {
    fun runNavigatorAction(fallbackError: String, action: () -> Any?) {
        val result = runCatching(action)
        result.exceptionOrNull()?.let {
            onActionError(it.cause?.message ?: it.message ?: fallbackError)
        }
    }

    Row(
        modifier = Modifier.padding(top = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        IconButton(
            onClick = { runNavigatorAction("Navigation failed") { navigator.goBack() } },
            enabled = navigator.canGoBack,
            modifier = Modifier.size(40.dp),
        ) {
            Text("←")
        }
        IconButton(
            onClick = { runNavigatorAction("Navigation failed") { navigator.goForward(urlInput) } },
            enabled = navigator.canGoForward,
            modifier = Modifier.size(40.dp),
        ) {
            Text("→")
        }
        IconButton(
            onClick = { runNavigatorAction("Refresh failed") { navigator.refresh() } },
            modifier = Modifier.size(40.dp),
        ) {
            Text("⟳")
        }
        IconButton(
            onClick = { runNavigatorAction("Stop failed") { navigator.stop() } },
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

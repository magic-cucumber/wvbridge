package sample.app

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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
                var url by remember { mutableStateOf("https://www.baidu.com") }
                val webViewState = rememberWebViewState(url)

                var dialog by remember {
                    mutableStateOf("")
                }

                LaunchedEffect(webViewState.state) {
                    val state = webViewState.state
                    if (state is LoadingState.LoadingEnd && !state.success) {
                        dialog = state.reason ?: "unknown error"
                    }
                }

                LaunchedEffect(webViewState.url) {
                    if (webViewState.url.isNotBlank() && webViewState.url != url) {
                        url = webViewState.url
                    }
                }

                Column(modifier = Modifier.fillMaxSize().padding(12.dp)) {
                    with(webViewState.state) {
                        if (this is LoadingState.Loading) {
                            LinearProgressIndicator(
                                progress = { this.progress },
                                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
                            )
                        }
                    }
                    BrowserToolbar(
                        urlInput = url,
                        onUrlInputChange = { url = it },
                        onSubmitUrl = { webViewState.navigator.loadUrl(url) },
                        navigator = webViewState.navigator,
                        isLoading = webViewState.state is LoadingState.Loading,
                    )
                    WebView(
                        state = webViewState,
                        modifier = Modifier.fillMaxSize().padding(top = 10.dp),
                    )
                }

                if (dialog.isNotBlank()) {
                    //TODO it will be hidden when running on desktop...
                    AlertDialog(
                        onDismissRequest = { dialog = "" },
                        title = { Text("Error!") },
                        confirmButton = { TextButton(onClick = {dialog = ""}) { Text("ok!") } },
                        text = { Text(dialog) }
                    )
                }
            }
        }
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

package sample.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import top.kagg886.wvbridge.LoadingState
import top.kagg886.wvbridge.WebView
import top.kagg886.wvbridge.WebViewController
import top.kagg886.wvbridge.WebViewNavigator

@Composable
internal fun BrowserScreen(
    url: String,
    onUrlChange: (String) -> Unit,
    webViewController: WebViewController<*>,
    onRunJavaScript: () -> Unit,
    onMessage: (title: String, message: String) -> Unit,
) {
    LaunchedEffect(webViewController.loadingState) {
        val loadingState = webViewController.loadingState
        if (loadingState is LoadingState.LoadingEnd && !loadingState.success) {
            onMessage("Error", loadingState.reason ?: "unknown error")
        }
    }

    LaunchedEffect(webViewController.url) {
        if (webViewController.url.isNotBlank() && webViewController.url != url) {
            onUrlChange(webViewController.url)
        }
    }

    Column(modifier = Modifier.fillMaxSize().padding(28.dp)) {
        val progress = remember(webViewController.loadingState) {
            (webViewController.loadingState as? LoadingState.Loading)?.progress ?: 0f
        }
        if (progress != 0f) {
            LinearProgressIndicator(
                progress = { progress },
                modifier = Modifier.fillMaxWidth().padding(top = 4.dp),
            )
        }
        BrowserToolbar(
            urlInput = url,
            onUrlInputChange = onUrlChange,
            onSubmitUrl = { webViewController.navigator.loadUrl(url) },
            onRunJavaScript = onRunJavaScript,
            navigator = webViewController.navigator,
            isLoading = webViewController.loadingState is LoadingState.Loading,
        )
        WebView(
            controller = webViewController,
            modifier = Modifier.fillMaxSize().padding(top = 10.dp),
        )
    }
}

@Composable
private fun BrowserToolbar(
    urlInput: String,
    onUrlInputChange: (String) -> Unit,
    onSubmitUrl: () -> Unit,
    onRunJavaScript: () -> Unit,
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
            Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
        }
        IconButton(
            onClick = { navigator.goForward() },
            enabled = navigator.canGoForward,
            modifier = Modifier.size(40.dp),
        ) {
            Icon(Icons.AutoMirrored.Filled.ArrowForward, contentDescription = "Forward")
        }
        IconButton(
            onClick = { navigator.refresh() },
            modifier = Modifier.size(40.dp),
        ) {
            Icon(Icons.Filled.Refresh, contentDescription = "Refresh")
        }
        IconButton(
            onClick = { navigator.stop() },
            enabled = isLoading,
            modifier = Modifier.size(40.dp),
        ) {
            Icon(Icons.Filled.Stop, contentDescription = "Stop")
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
        PlatformActionsMenu {
            item {
                icon(Icons.Filled.Code)
                text("Run JavaScript")
                onClick(onRunJavaScript)
            }
        }
    }
}

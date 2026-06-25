package sample.app

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.sp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.launch
import kotlinx.datetime.format
import kotlinx.datetime.format.DateTimeComponents
import kotlinx.datetime.format.DateTimeFormat
import kotlinx.datetime.format.DateTimeFormatBuilder
import top.kagg886.pmf.ui.component.scroll.VerticalScrollbar
import top.kagg886.pmf.ui.component.scroll.rememberScrollbarAdapter
import top.kagg886.wvbridge.LoadingState
import top.kagg886.wvbridge.WebView
import top.kagg886.wvbridge.WebViewController
import top.kagg886.wvbridge.WebViewNavigator
import top.kagg886.wvbridge.bridge.CloseHandle
import top.kagg886.wvbridge.js.evaluateScriptValue
import top.kagg886.wvbridge.js.registerWebMessageHandler
import top.kagg886.wvbridge.js.protocol.JSValue
import top.kagg886.wvbridge.util.LoggerReceiver
import kotlin.time.Clock

internal fun receiver(handle: (level: LoggerReceiver.Level, message: String) -> Unit): LoggerReceiver =
    LoggerReceiver { level, tag, message ->
        val format = DateTimeComponents.Format {
            year()
            chars("-")
            monthNumber()
            chars("-")
            day()
            chars(" ")
            hour()
            chars(":")
            minute()
            chars(":")
            second()
        }

        fun String.truncate(length: Int) = when (this.length) {
            in 0..length -> this
            else -> substring(0, length - 3) + "..."
        }


        val time = "${Clock.System.now().format(format)} ${level.toString().padEnd(7)}"
        val tag = tag.truncate(16).padEnd(16)
        handle(
            level, "$time: [$tag] - $message"
        )
    }


@Composable
internal fun BrowserScreen(
    url: String,
    onUrlChange: (String) -> Unit,
    webViewController: WebViewController<*>,
    onRunJavaScript: () -> Unit,
    onMessage: (title: String, message: String) -> Unit,
) {
    val scope = rememberCoroutineScope()
    var documentStartHook by remember(webViewController) { mutableStateOf<CloseHandle?>(null) }
    var messageBridgeHandle by remember(webViewController) { mutableStateOf<CloseHandle?>(null) }
    var sendMessageCount by remember(webViewController) { mutableStateOf(0) }
    val logs = remember { mutableStateListOf<String>() }

    fun appendLog(message: String) {
        logs += message
        if (logs.size > MaxLogLines) {
            logs.removeRange(0, logs.size - MaxLogLines)
        }
    }

    DisposableEffect(Unit) {
        val rc = receiver { level, line ->
            println(line)
            if (level >= LoggerReceiver.Level.INFO)
            scope.launch {
                appendLog(line.takeWhile { it != '\n' })
            }
        }
        LoggerReceiver.register(rc)
        onDispose {
            LoggerReceiver.unregister(rc)
        }
    }

    DisposableEffect(webViewController) {
        onDispose {
            documentStartHook?.close()
            documentStartHook = null
            messageBridgeHandle?.close()
            messageBridgeHandle = null
        }
    }

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

    LaunchedEffect(sendMessageCount) {
        if (sendMessageCount > 0) {
            onMessage("Send Message", "sendMessageCount changed to $sendMessageCount")
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
            onInstallDocumentStartHook = {
                scope.launch {
                    val previousHook = documentStartHook
                    val result = runCatching {
                        webViewController.bridge.registerDocumentStartHook(DocumentStartHookScript)
                    }
                    result.onSuccess { hook ->
                        previousHook?.close()
                        documentStartHook = hook
                        onMessage("Document Start Hook", "Installed. Reload the page to run the hook.")
                    }.onFailure { error ->
                        onMessage("Document Start Hook", error.message ?: error.toString())
                    }
                }
            },
            onRemoveDocumentStartHook = {
                runCatching {
                    documentStartHook?.close()
                }.onSuccess {
                    documentStartHook = null
                    onMessage("Document Start Hook", "Removed.")
                }.onFailure { error ->
                    onMessage("Document Start Hook", error.message ?: error.toString())
                }
            },
            onRegisterMessageBridge = {
                scope.launch {
                    val previousHandle = messageBridgeHandle
                    val result = runCatching {
                        webViewController.bridge.registerWebMessageHandler(SampleMessageType) { value ->
                            scope.launch {
                                appendLog("JS message [$SampleMessageType]: ${value.formatForDisplay()}")
                                if (value is JSValue.ScriptObject && value.type == "number") {
                                    sendMessageCount += 1
                                }
                            }
                        }
                    }
                    result.onSuccess { handle ->
                        previousHandle?.close()
                        messageBridgeHandle = handle
                        appendLog("Message bridge registered for type '$SampleMessageType'.")
                    }.onFailure { error ->
                        onMessage("Message Bridge", error.message ?: error.toString())
                    }
                }
            },
            onUnregisterMessageBridge = {
                runCatching {
                    messageBridgeHandle?.close()
                }.onSuccess {
                    messageBridgeHandle = null
                    appendLog("Message bridge unregistered.")
                }.onFailure { error ->
                    onMessage("Message Bridge", error.message ?: error.toString())
                }
            },
            onSendMessage = {
                scope.launch {
                    val result = runCatching {
                        webViewController.bridge.evaluateScriptValue(
                            """
                                window.wvbridge.postMessage("$SampleMessageType", 1);
                                return true;
                            """.trimIndent()
                        )
                    }
                    result.onFailure { error ->
                        onMessage("Send Message", error.message ?: error.toString())
                    }
                }
            },
            navigator = webViewController.navigator,
            isLoading = webViewController.loadingState is LoadingState.Loading,
            isMessageBridgeRegistered = messageBridgeHandle != null,
        )
        WebView(
            controller = webViewController,
            modifier = Modifier.fillMaxWidth().weight(1f).padding(top = 10.dp),
        )
        LogPanel(
            logs = logs,
            onClear = { logs.clear() },
            modifier = Modifier.fillMaxWidth().padding(top = 10.dp).weight(0.32f),
        )
    }
}

@Composable
private fun BrowserToolbar(
    urlInput: String,
    onUrlInputChange: (String) -> Unit,
    onSubmitUrl: () -> Unit,
    onRunJavaScript: () -> Unit,
    onInstallDocumentStartHook: () -> Unit,
    onRemoveDocumentStartHook: () -> Unit,
    onRegisterMessageBridge: () -> Unit,
    onUnregisterMessageBridge: () -> Unit,
    onSendMessage: () -> Unit,
    navigator: WebViewNavigator,
    isLoading: Boolean,
    isMessageBridgeRegistered: Boolean,
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
            item {
                icon(Icons.Filled.Add)
                text("Insert document start hook")
                onClick(onInstallDocumentStartHook)
            }
            item {
                icon(Icons.Filled.Delete)
                text("Remove document start hook")
                onClick(onRemoveDocumentStartHook)
            }
            item {
                icon(Icons.Filled.Add)
                text(if (isMessageBridgeRegistered) "Re-register message bridge" else "Register message bridge")
                onClick(onRegisterMessageBridge)
            }
            item {
                icon(Icons.Filled.Delete)
                text("Unregister message bridge")
                onClick(onUnregisterMessageBridge)
            }
            item {
                icon(Icons.Filled.Code)
                text("Send message")
                onClick(onSendMessage)
            }
        }
    }
}

@Composable
private fun LogPanel(
    logs: List<String>,
    onClear: () -> Unit,
    modifier: Modifier = Modifier,
) {
    val listState = rememberScrollState()

    LaunchedEffect(logs.size) {
        if (logs.isNotEmpty()) {
            listState.scrollTo(listState.maxValue)
        }
    }

    Surface(
        modifier = modifier,
        tonalElevation = 2.dp,
        shadowElevation = 1.dp,
    ) {
        Column(modifier = Modifier.fillMaxSize().padding(10.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = "Info Logs",
                    modifier = Modifier.weight(1f),
                    fontSize = 12.sp,
                )
                TextButton(onClick = onClear) {
                    Text("Clear", fontSize = 12.sp)
                }
            }
            Box(modifier = Modifier.fillMaxSize().padding(top = 4.dp)) {
                SelectionContainer {
                    Column(Modifier.fillMaxSize().verticalScroll(listState).horizontalScroll(rememberScrollState())) {
                        for (log in logs) {
                            Text(
                                text = log,
                                fontSize = 11.sp,
                                lineHeight = 14.sp,
                            )
                        }
                    }
                }
                VerticalScrollbar(
                    adapter = rememberScrollbarAdapter(listState),
                    modifier = Modifier.align(Alignment.CenterEnd).fillMaxHeight().width(8.dp),
                )
            }
        }
    }
}

private const val SampleMessageType = "sample"
private const val MaxLogLines = 200

private val DocumentStartHookScript = """
    const appendHookDiv = () => {
        const div = document.createElement("div");
        div.textContent = "Inserted by wvbridge document-start hook";
        div.style.cssText = [
            "position: fixed",
            "right: 16px",
            "bottom: 16px",
            "z-index: 2147483647",
            "padding: 10px 12px",
            "background: #0f766e",
            "color: white",
            "font: 14px sans-serif",
            "border-radius: 6px",
            "box-shadow: 0 4px 14px rgba(0,0,0,.25)"
        ].join(";");
        document.body.appendChild(div);
    };

    if (document.readyState === "loading") {
        document.addEventListener("DOMContentLoaded", appendHookDiv, { once: true });
    } else {
        appendHookDiv();
    }
""".trimIndent()

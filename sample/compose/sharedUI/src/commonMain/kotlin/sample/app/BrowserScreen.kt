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
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
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
import top.kagg886.wvbridge.js.postMessage
import top.kagg886.wvbridge.js.postMessageAndReceiveResult
import top.kagg886.wvbridge.js.registerWebMessageHandler
import top.kagg886.wvbridge.js.protocol.JSValue
import top.kagg886.wvbridge.util.LoggerReceiver
import kotlin.time.Clock
import kotlin.time.Duration.Companion.seconds

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
    var javaScriptTestListenersInstalled by remember(webViewController) { mutableStateOf(false) }
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
            onToggleDocumentStartHook = {
                if (documentStartHook == null) {
                    scope.launch {
                        val result = runCatching {
                            webViewController.bridge.registerDocumentStartHook(DocumentStartHookScript)
                        }
                        result.onSuccess { hook ->
                            documentStartHook = hook
                            onMessage("Document Start Hook", "Installed. Reload the page to run the hook.")
                        }.onFailure { error ->
                            onMessage("Document Start Hook", error.message ?: error.toString())
                        }
                    }
                } else {
                    runCatching {
                        documentStartHook?.close()
                    }.onSuccess {
                        documentStartHook = null
                        onMessage("Document Start Hook", "Removed.")
                    }.onFailure { error ->
                        onMessage("Document Start Hook", error.message ?: error.toString())
                    }
                }
            },
            onToggleMessageBridge = {
                if (messageBridgeHandle == null) {
                    scope.launch {
                        val result = runCatching {
                            webViewController.bridge.registerWebMessageHandler(SampleMessageType) { values ->
                                scope.launch {
                                    val value = values.firstOrNull() ?: JSValue.Undefined
                                    appendLog("JS message [$SampleMessageType]: ${value.formatForDisplay()}")
                                    if (value is JSValue.ScriptObject && value.type == "number") {
                                        sendMessageCount += 1
                                    }
                                }
                            }
                        }
                        result.onSuccess { handle ->
                            messageBridgeHandle = handle
                            appendLog("Message bridge registered for type '$SampleMessageType'.")
                        }.onFailure { error ->
                            onMessage("Message Bridge", error.message ?: error.toString())
                        }
                    }
                } else {
                    runCatching {
                        messageBridgeHandle?.close()
                    }.onSuccess {
                        messageBridgeHandle = null
                        appendLog("Message bridge unregistered.")
                    }.onFailure { error ->
                        onMessage("Message Bridge", error.message ?: error.toString())
                    }
                }
            },
            onEvaluateTypedValue = {
                scope.launch {
                    val result = runCatching {
                        webViewController.bridge.evaluateScriptValue(
                            """
                                return {
                                    title: document.title,
                                    href: location.href,
                                    readyState: document.readyState,
                                };
                            """.trimIndent()
                        )
                    }
                    result.onSuccess { value ->
                        appendLog("evaluateScriptValue result: ${value.formatForDisplay()}")
                    }.onFailure { error ->
                        onMessage("Evaluate Script Value", error.message ?: error.toString())
                    }
                }
            },
            onInstallJavaScriptTestListeners = {
                scope.launch {
                    val result = runCatching {
                        webViewController.bridge.evaluateScriptValue(JavaScriptBridgeTestListenersScript)
                    }
                    result.onSuccess { value ->
                        javaScriptTestListenersInstalled = true
                        appendLog("JavaScript test listeners installed: ${value.formatForDisplay()}")
                    }.onFailure { error ->
                        onMessage("JavaScript Test Listeners", error.message ?: error.toString())
                    }
                }
            },
            onPostMessageToJavaScript = {
                scope.launch {
                    val result = runCatching {
                        webViewController.bridge.postMessage(
                            NativeNotifyMessageType,
                            JSValue.Serializable(
                                JsonObject(
                                    mapOf(
                                        "from" to JsonPrimitive("kotlin"),
                                        "count" to JsonPrimitive(sendMessageCount),
                                    )
                                )
                            )
                        )
                    }
                    result.onSuccess {
                        appendLog("postMessage dispatched to '$NativeNotifyMessageType'.")
                    }.onFailure { error ->
                        onMessage("Post Message", error.message ?: error.toString())
                    }
                }
            },
            onPostMessageAndReceiveResult = {
                scope.launch {
                    val result = runCatching {
                        webViewController.bridge.postMessageAndReceiveResult(
                            NativeRequestMessageType,
                            5.seconds,
                            JSValue.Serializable(
                                JsonObject(
                                    mapOf(
                                        "question" to JsonPrimitive("document-info"),
                                        "count" to JsonPrimitive(sendMessageCount),
                                    )
                                )
                            )
                        )
                    }
                    result.onSuccess { value ->
                        val message = value.formatForDisplay()
                        appendLog("postMessageAndReceiveResult result: $message")
                        onMessage("Receive Result", message)
                    }.onFailure { error ->
                        onMessage("Receive Result", error.message ?: error.toString())
                    }
                }
            },
            onSendMessageFromJavaScript = {
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
            isDocumentStartHookInstalled = documentStartHook != null,
            isMessageBridgeRegistered = messageBridgeHandle != null,
            areJavaScriptTestListenersInstalled = javaScriptTestListenersInstalled,
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
    onToggleDocumentStartHook: () -> Unit,
    onToggleMessageBridge: () -> Unit,
    onEvaluateTypedValue: () -> Unit,
    onInstallJavaScriptTestListeners: () -> Unit,
    onPostMessageToJavaScript: () -> Unit,
    onPostMessageAndReceiveResult: () -> Unit,
    onSendMessageFromJavaScript: () -> Unit,
    navigator: WebViewNavigator,
    isLoading: Boolean,
    isDocumentStartHookInstalled: Boolean,
    isMessageBridgeRegistered: Boolean,
    areJavaScriptTestListenersInstalled: Boolean,
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
                icon(Icons.Filled.Code)
                text("Evaluate typed value")
                onClick(onEvaluateTypedValue)
            }
            item {
                icon(if (isDocumentStartHookInstalled) Icons.Filled.Delete else Icons.Filled.Add)
                text(if (isDocumentStartHookInstalled) "Remove document start hook" else "Insert document start hook")
                onClick(onToggleDocumentStartHook)
            }
            item {
                icon(if (isMessageBridgeRegistered) Icons.Filled.Delete else Icons.Filled.Add)
                text(if (isMessageBridgeRegistered) "Unregister message bridge" else "Register message bridge")
                onClick(onToggleMessageBridge)
            }
            item {
                icon(Icons.Filled.Code)
                text(if (areJavaScriptTestListenersInstalled) "Refresh JavaScript test listeners" else "Install JavaScript test listeners")
                onClick(onInstallJavaScriptTestListeners)
            }
            item {
                icon(Icons.Filled.Code)
                text("Send JS message to Kotlin")
                onClick(onSendMessageFromJavaScript)
            }
            item {
                icon(Icons.Filled.Code)
                text("Post message to JavaScript")
                onClick(onPostMessageToJavaScript)
            }
            item {
                icon(Icons.Filled.Code)
                text("Post message and receive result")
                onClick(onPostMessageAndReceiveResult)
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
private const val NativeNotifyMessageType = "native:notify"
private const val NativeRequestMessageType = "native:request"
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

private val JavaScriptBridgeTestListenersScript = """
    if (!window.__wvbridgeSampleListenersInstalled) {
        window.__wvbridgeSampleListenersInstalled = true;

        window.wvbridge.addEventListener("$NativeNotifyMessageType", (payload) => {
            console.log("wvbridge native notify", payload);
            const id = "wvbridge-native-notify-log";
            let div = document.getElementById(id);
            if (!div) {
                div = document.createElement("div");
                div.id = id;
                div.style.cssText = [
                    "position: fixed",
                    "left: 16px",
                    "bottom: 16px",
                    "z-index: 2147483647",
                    "padding: 10px 12px",
                    "background: #1d4ed8",
                    "color: white",
                    "font: 14px sans-serif",
                    "border-radius: 6px",
                    "box-shadow: 0 4px 14px rgba(0,0,0,.25)"
                ].join(";");
                document.body.appendChild(div);
            }
            div.textContent = "Native message: " + JSON.stringify(payload);
        });

        window.wvbridge.addEventListener("$NativeRequestMessageType", (payload, reply) => {
            reply({
                title: document.title,
                href: location.href,
                readyState: document.readyState,
                payload,
            });
        });
    }

    return {
        notifyType: "$NativeNotifyMessageType",
        requestType: "$NativeRequestMessageType",
        installed: window.__wvbridgeSampleListenersInstalled,
    };
""".trimIndent()

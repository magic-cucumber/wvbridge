package sample.app

import androidx.compose.runtime.Composable
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.rememberCoroutineScope
import androidx.navigation3.runtime.NavEntry
import androidx.navigation3.runtime.NavMetadataKey
import androidx.navigation3.runtime.entryProvider
import androidx.navigation3.runtime.get
import androidx.navigation3.runtime.metadata
import androidx.navigation3.scene.OverlayScene
import androidx.navigation3.scene.Scene
import androidx.navigation3.scene.SceneStrategy
import androidx.navigation3.scene.SceneStrategyScope
import androidx.navigation3.scene.SinglePaneSceneStrategy
import kotlinx.coroutines.launch
import top.kagg886.wvbridge.WebViewController
import top.kagg886.wvbridge.js.evaluateScriptValue

internal sealed interface AppRoute {
    data object Main : AppRoute
    data object RunJavaScriptDialog : AppRoute
    data class MessageDialog(val title: String, val message: String) : AppRoute
}

internal fun appSceneStrategies(): List<SceneStrategy<AppRoute>> =
    listOf(PlatformDialogSceneStrategy(), SinglePaneSceneStrategy())

@Composable
internal fun appEntryProvider(
    backStack: SnapshotStateList<AppRoute>,
    url: String,
    onUrlChange: (String) -> Unit,
    webViewController: WebViewController<*>,
    dismissDialog: () -> Unit,
): (AppRoute) -> NavEntry<AppRoute> {
    val scope = rememberCoroutineScope()

    return entryProvider {
        entry<AppRoute.Main> {
            BrowserScreen(
                url = url,
                onUrlChange = onUrlChange,
                webViewController = webViewController,
                onRunJavaScript = { backStack.add(AppRoute.RunJavaScriptDialog) },
                onMessage = { title, message ->
                    dismissDialog()
                    backStack.add(AppRoute.MessageDialog(title, message))
                },
            )
        }
        entry<AppRoute.RunJavaScriptDialog>(
            metadata = PlatformDialogSceneStrategy.dialog("Run JavaScript")
        ) {
            JavaScriptDialogScreen(
                onDismissRequest = dismissDialog,
                onConfirm = { script ->
                    dismissDialog()
                    scope.launch {
                        val result = runCatching {
                            webViewController.bridge.evaluateScriptValue(script).formatForDisplay()
                        }.getOrElse { it.message ?: it.toString() }
                        backStack.add(AppRoute.MessageDialog("JavaScript Result", result))
                    }
                },
            )
        }
        entry<AppRoute.MessageDialog>(
            metadata = { route -> PlatformDialogSceneStrategy.dialog(route.title) }
        ) { route ->
            MessageDialogScreen(
                title = route.title,
                message = route.message,
                onDismissRequest = dismissDialog,
            )
        }
    }
}

private class PlatformDialogScene<T : Any>(
    override val key: Any,
    private val entry: NavEntry<T>,
    override val previousEntries: List<NavEntry<T>>,
    override val overlaidEntries: List<NavEntry<T>>,
    private val title: String,
    private val onDismissRequest: () -> Unit,
) : OverlayScene<T> {
    override val entries: List<NavEntry<T>> = listOf(entry)

    override val content: @Composable () -> Unit = {
        PlatformDialog(
            title = title,
            onDismissRequest = onDismissRequest,
        ) {
            entry.Content()
        }
    }
}

private class PlatformDialogSceneStrategy<T : Any> : SceneStrategy<T> {
    override fun SceneStrategyScope<T>.calculateScene(entries: List<NavEntry<T>>): Scene<T>? {
        val lastEntry = entries.lastOrNull()
        val dialogTitle = lastEntry?.metadata?.get(DialogTitleKey) ?: return null
        return PlatformDialogScene(
            key = lastEntry.contentKey,
            entry = lastEntry,
            previousEntries = entries.dropLast(1),
            overlaidEntries = entries.dropLast(1),
            title = dialogTitle,
            onDismissRequest = onBack,
        )
    }

    companion object {
        object DialogTitleKey : NavMetadataKey<String>

        fun dialog(title: String = ""): Map<String, Any> = metadata {
            put(DialogTitleKey, title)
        }
    }
}

package sample.app

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.navigation3.ui.NavDisplay
import top.kagg886.wvbridge.rememberWebViewController

@Composable
fun App() {
    MaterialTheme {
        var url by remember { mutableStateOf("https://www.baidu.com") }
        val webViewController = rememberWebViewController(url)
        val backStack = remember { mutableStateListOf<AppRoute>(AppRoute.Main) }
        fun dismissDialog() {
            if (backStack.size > 1) {
                backStack.removeLastOrNull()
            }
        }

        Surface(modifier = Modifier.fillMaxSize()) {
            NavDisplay(
                backStack = backStack,
                modifier = Modifier.fillMaxSize(),
                sceneStrategies = appSceneStrategies(),
                onBack = ::dismissDialog,
                entryProvider = appEntryProvider(
                    backStack = backStack,
                    url = url,
                    onUrlChange = { url = it },
                    webViewController = webViewController,
                    dismissDialog = ::dismissDialog,
                ),
            )
        }
    }
}

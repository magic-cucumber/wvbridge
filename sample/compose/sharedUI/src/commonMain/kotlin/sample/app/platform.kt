package sample.app

import androidx.compose.runtime.Composable

@Composable
internal expect fun PlatformDialog(
    title: String,
    onDismissRequest: () -> Unit,
    content: @Composable () -> Unit,
)

@Composable
internal expect fun PlatformActionsMenu(
    onRunJavaScript: () -> Unit,
)

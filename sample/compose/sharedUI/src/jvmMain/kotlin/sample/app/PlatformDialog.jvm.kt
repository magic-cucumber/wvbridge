package sample.app

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.awt.SwingPanel
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Window
import androidx.compose.ui.window.rememberWindowState
import java.awt.Insets
import javax.swing.JButton
import javax.swing.JMenuItem
import javax.swing.JPopupMenu

@Composable
internal actual fun PlatformDialog(
    title: String,
    onDismissRequest: () -> Unit,
    content: @Composable () -> Unit,
) {
    Window(
        onCloseRequest = onDismissRequest,
        title = title,
        state = rememberWindowState(width = 560.dp, height = 420.dp),
    ) {
        MaterialTheme {
            Box(
                modifier = Modifier.fillMaxSize().padding(16.dp),
            ) {
                content()
            }
        }
    }
}

@Composable
internal actual fun PlatformActionsMenu(
    onRunJavaScript: () -> Unit,
) {
    SwingPanel(
        modifier = Modifier.size(40.dp),
        factory = {
            JButton("...").apply {
                margin = Insets(0, 0, 0, 0)
                isFocusable = false
                toolTipText = "Actions"
            }
        },
        update = { button ->
            button.actionListeners.forEach(button::removeActionListener)
            button.addActionListener {
                JPopupMenu().apply {
                    add(
                        JMenuItem("Run JavaScript").apply {
                            addActionListener { onRunJavaScript() }
                        }
                    )
                }.show(button, 0, button.height)
            }
        },
    )
}

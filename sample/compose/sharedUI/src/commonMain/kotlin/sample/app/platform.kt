package sample.app

import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.vector.ImageVector

@Composable
internal expect fun PlatformDialog(
    title: String,
    onDismissRequest: () -> Unit,
    content: @Composable () -> Unit,
)

@Composable
internal expect fun PlatformActionsMenu(
    content: PlatformActionsMenuScope.() -> Unit,
)

internal class PlatformActionsMenuScope {
    private val _items = mutableListOf<PlatformActionsMenuItem>()
    val items: List<PlatformActionsMenuItem> get() = _items

    fun item(content: PlatformActionsMenuItemScope.() -> Unit) {
        _items += PlatformActionsMenuItemScope().apply(content).build()
    }
}

internal class PlatformActionsMenuItemScope {
    private var icon: ImageVector? = null
    private var text: String? = null
    private var onClick: (() -> Unit)? = null

    fun icon(icon: ImageVector) {
        this.icon = icon
    }

    fun text(text: String) {
        this.text = text
    }

    fun onClick(action: () -> Unit) {
        this.onClick = action
    }

    fun build(): PlatformActionsMenuItem {
        return PlatformActionsMenuItem(
            icon = icon,
            text = requireNotNull(text) { "PlatformActionsMenu item requires text()." },
            onClick = requireNotNull(onClick) { "PlatformActionsMenu item requires onClick {}." },
        )
    }
}

internal data class PlatformActionsMenuItem(
    val icon: ImageVector?,
    val text: String,
    val onClick: () -> Unit,
)

package sample.app

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text

@Composable
internal actual fun PlatformDialog(
    title: String,
    onDismissRequest: () -> Unit,
    content: @Composable () -> Unit,
) {
    Dialog(onDismissRequest = onDismissRequest) {
        content()
    }
}

@Composable
internal actual fun PlatformActionsMenu(
    content: PlatformActionsMenuScope.() -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val items = PlatformActionsMenuScope().apply(content).items

    IconButton(
        onClick = { expanded = true },
        modifier = Modifier.size(40.dp),
    ) {
        Icon(Icons.Filled.MoreVert, contentDescription = "Actions")
    }
    DropdownMenu(
        expanded = expanded,
        onDismissRequest = { expanded = false },
    ) {
        items.forEach { item ->
            DropdownMenuItem(
                text = { Text(item.text) },
                onClick = {
                    expanded = false
                    item.onClick()
                },
                leadingIcon = item.icon?.let { icon ->
                    {
                        Icon(icon, contentDescription = item.text)
                    }
                },
            )
        }
    }
}

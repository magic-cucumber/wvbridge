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
    onRunJavaScript: () -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

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
        DropdownMenuItem(
            text = { Text("Run JavaScript") },
            onClick = {
                expanded = false
                onRunJavaScript()
            },
        )
    }
}

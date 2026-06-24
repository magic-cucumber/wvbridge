package sample.app

import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
internal fun JavaScriptDialogScreen(
    onDismissRequest: () -> Unit,
    onConfirm: (String) -> Unit,
) {
    var script by remember {
        mutableStateOf(
            """
            return {
                title: document.title,
                href: location.href,
            }
            """.trimIndent()
        )
    }

    AppDialog(
        title = "Run JavaScript",
    ) {
        OutlinedTextField(
            value = script,
            onValueChange = { script = it },
            modifier = Modifier.fillMaxWidth().heightIn(min = 220.dp),
            minLines = 8,
            maxLines = 16,
            label = { Text("JavaScript") },
        )
        DialogActions(
            onCancel = onDismissRequest,
            onConfirm = { onConfirm(script) },
        )
    }
}

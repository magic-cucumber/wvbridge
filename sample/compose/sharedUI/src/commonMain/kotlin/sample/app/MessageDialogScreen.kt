package sample.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
internal fun MessageDialogScreen(
    title: String,
    message: String,
    onDismissRequest: () -> Unit,
) {
    AppDialog(
        title = title,
    ) {
        Text(
            text = message,
            modifier = Modifier.fillMaxWidth()
                .heightIn(max = 280.dp)
                .verticalScroll(rememberScrollState()),
        )
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp, Alignment.End),
        ) {
            Button(onClick = onDismissRequest) {
                Text("OK")
            }
        }
    }
}

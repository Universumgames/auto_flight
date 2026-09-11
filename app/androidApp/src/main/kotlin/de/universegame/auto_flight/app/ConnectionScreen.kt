package de.universegame.auto_flight.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun ConnectionScreen(
    viewModel: AppViewModel,
    onNext: () -> Unit,
) {
    LaunchedEffect(Unit) {
        viewModel.wizardStep = ConfigurationState.CONNECTION
    }

    val state = viewModel.state
    val connectionItems = listOf(ConnectionItems.baseStationItem(state)) + ConnectionItems.planeItems(state)
    val allConnected = ConnectionItems.totalIsConnected(state)

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("Connection Overview", style = MaterialTheme.typography.titleLarge)
        Text("Live connection status and the sub-tasks that make each link ready.")

        OutlinedTextField(
            value = viewModel.host,
            onValueChange = { viewModel.host = it },
            label = { Text("Base station host (e.g. 192.168.4.1)") },
            modifier = Modifier.fillMaxWidth(),
            singleLine = true,
        )
        Button(onClick = { viewModel.connect() }, modifier = Modifier.fillMaxWidth()) {
            Text("Connect")
        }

        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            items(connectionItems) { item -> ConnectionItemCard(item) }
        }

        Button(
            onClick = onNext,
            enabled = allConnected,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Text("Next Step")
        }
    }
}

@Composable
private fun ConnectionItemCard(item: ConnectionItem) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    StatusIcon(connected = ConnectionItems.isConnected(item.status), size = 18.dp)
                    Text(item.label, style = MaterialTheme.typography.titleMedium)
                }
                Text(ConnectionItems.formatStatus(item.status))
            }
            for (subTask in item.subTasks) {
                SubTaskRow(subTask)
            }
        }
    }
}

@Composable
private fun SubTaskRow(subTask: SubTask) {
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
        StatusIcon(connected = subTask.state == SubTaskState.DONE, size = 14.dp)
        Text(subTask.label, style = MaterialTheme.typography.bodyMedium)
    }
}

@Composable
private fun StatusIcon(connected: Boolean, size: androidx.compose.ui.unit.Dp) {
    if (connected) {
        Icon(Icons.Filled.Check, contentDescription = null, modifier = Modifier.size(size))
    } else {
        CircularProgressIndicator(modifier = Modifier.size(size), strokeWidth = 2.dp)
    }
}

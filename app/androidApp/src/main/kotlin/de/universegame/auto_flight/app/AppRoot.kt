package de.universegame.auto_flight.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController

private object Routes {
    const val CONNECTION = "connection"
    const val AREA = "area"
    const val ROUTE = "route"
}

@Composable
fun AppRoot() {
    MaterialTheme {
        val viewModel: AppViewModel = viewModel()
        val navController = rememberNavController()
        val state = viewModel.state
        val currentRoute = navController.currentBackStackEntryAsState().value?.destination?.route

        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("Auto Flight") },
                    actions = { ConnectionStatusButton(state) },
                )
            },
        ) { padding ->
            Column(modifier = Modifier.fillMaxSize().padding(padding)) {
                WarningBanner(state = state, isConnectionScreen = currentRoute == Routes.CONNECTION)

                Row(modifier = Modifier.fillMaxSize()) {
                    Box(modifier = Modifier.weight(1f).fillMaxSize()) {
                        NavHost(navController = navController, startDestination = Routes.CONNECTION) {
                            composable(Routes.CONNECTION) {
                                ConnectionScreen(viewModel) { navController.navigate(Routes.AREA) }
                            }
                            composable(Routes.AREA) {
                                AreaScreen(viewModel) { navController.navigate(Routes.ROUTE) }
                            }
                            composable(Routes.ROUTE) {
                                RouteScreen(viewModel)
                            }
                        }
                    }
                    StepPanel(wizardStep = viewModel.wizardStep, modifier = Modifier.width(160.dp))
                }
            }
        }
    }
}

@Composable
private fun ConnectionStatusButton(state: AppState) {
    var open by remember { mutableStateOf(false) }
    val connected = ConnectionItems.totalIsConnected(state)

    Box {
        IconButton(onClick = { open = true }) {
            if (connected) {
                Icon(Icons.Filled.Check, contentDescription = "Connected", tint = Color(0xFF16A34A))
            } else {
                CircularProgressIndicator(modifier = Modifier.size(20.dp), strokeWidth = 2.dp)
            }
        }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }) {
            Column(modifier = Modifier.padding(12.dp).width(280.dp)) {
                val connectionItems = listOf(ConnectionItems.baseStationItem(state)) + ConnectionItems.planeItems(state)
                for (item in connectionItems) {
                    Text(
                        "${item.label}: ${ConnectionItems.formatStatus(item.status)}",
                        style = MaterialTheme.typography.titleSmall,
                        modifier = Modifier.padding(top = 8.dp),
                    )
                    for (subTask in item.subTasks) {
                        Text(
                            "• ${subTask.label}",
                            style = MaterialTheme.typography.bodySmall,
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun WarningBanner(state: AppState, isConnectionScreen: Boolean) {
    val message: String?
    val isError: Boolean
    when {
        ConnectionItems.motorControllerDisconnectedError(state) -> {
            message = "Motor controller disconnected"
            isError = true
        }
        ConnectionItems.gpsPlaneUnavailableError(state) && !isConnectionScreen -> {
            message = "No GPS position available for the plane"
            isError = true
        }
        ConnectionItems.autopilotDisabledWarning(state) -> {
            message = "Autopilot control is disabled — manual override active"
            isError = false
        }
        else -> {
            message = null
            isError = false
        }
    }

    if (message != null) {
        Surface(color = if (isError) Color(0xFFFECACA) else Color(0xFFFDE68A)) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp, vertical = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(Icons.Filled.Warning, contentDescription = null, tint = if (isError) Color(0xFFDC2626) else Color(0xFFB45309))
                Text(message, color = if (isError) Color(0xFF7F1D1D) else Color(0xFF78350F))
            }
        }
    }
}

private data class StepDescriptor(val label: String, val step: ConfigurationState)

private val steps = listOf(
    StepDescriptor("Establish Connection", ConfigurationState.CONNECTION),
    StepDescriptor("Select Area", ConfigurationState.AREA_SELECTION),
    StepDescriptor("Review Route", ConfigurationState.ROUTE_APPROVAL),
    StepDescriptor("Prepare for flight", ConfigurationState.STARTING),
    StepDescriptor("Observe flight", ConfigurationState.FLYING),
    StepDescriptor("Finishing", ConfigurationState.FINISHING),
)

@Composable
private fun StepPanel(wizardStep: ConfigurationState, modifier: Modifier = Modifier) {
    Surface(modifier = modifier.fillMaxSize(), color = MaterialTheme.colorScheme.surfaceVariant) {
        LazyColumn(modifier = Modifier.padding(12.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            item {
                Text("Flight Planner", style = MaterialTheme.typography.titleMedium)
            }
            items(steps) { descriptor ->
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    when {
                        wizardStep.ordinal > descriptor.step.ordinal ->
                            Icon(Icons.Filled.Check, contentDescription = null, modifier = Modifier.size(16.dp))
                        wizardStep.ordinal == descriptor.step.ordinal ->
                            CircularProgressIndicator(modifier = Modifier.size(16.dp), strokeWidth = 2.dp)
                        else -> Box(modifier = Modifier.size(16.dp))
                    }
                    Text(descriptor.label, style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}

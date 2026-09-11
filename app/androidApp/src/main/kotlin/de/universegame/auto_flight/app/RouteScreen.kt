package de.universegame.auto_flight.app

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.delay
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.overlay.Marker
import org.osmdroid.views.overlay.Polyline

private const val MAX_ROUTE_POLL_ATTEMPTS = 50

@Composable
fun RouteScreen(viewModel: AppViewModel) {
    LaunchedEffect(Unit) {
        viewModel.wizardStep = ConfigurationState.ROUTE_APPROVAL
    }

    var plannedRoute by remember { mutableStateOf(emptyList<Coordinate>()) }

    LaunchedEffect(Unit) {
        var tries = 0
        while (plannedRoute.isEmpty() && tries < MAX_ROUTE_POLL_ATTEMPTS) {
            val route = fetchRouteSuspending(viewModel.repository)
            if (route.isNotEmpty()) plannedRoute = route
            tries++
            delay(1000)
        }
    }

    val state = viewModel.state
    val plane = state.planes[DEFAULT_PLANE_ID]
    val flightHistory = plane?.flightRoute.orEmpty()

    Column(modifier = Modifier.fillMaxSize()) {
        Text(
            "Route Preview",
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(16.dp),
        )

        OsmMapView(modifier = Modifier.weight(1f)) { mapView ->
            mapView.overlays.removeAll { it is Polyline || it is Marker }

            if (plannedRoute.size > 1) {
                mapView.overlays.add(
                    Polyline(mapView).apply {
                        setPoints(plannedRoute.map { GeoPoint(it.latitude, it.longitude) })
                        outlinePaint.color = android.graphics.Color.parseColor("#2563EB")
                        outlinePaint.strokeWidth = 6f
                    },
                )
            }
            if (flightHistory.size > 1) {
                mapView.overlays.add(
                    Polyline(mapView).apply {
                        setPoints(flightHistory.map { GeoPoint(it.latitude, it.longitude) })
                        outlinePaint.color = android.graphics.Color.parseColor("#DC2626")
                        outlinePaint.strokeWidth = 6f
                    },
                )
            }

            state.basePosition?.let { pos ->
                mapView.overlays.add(
                    Marker(mapView).apply {
                        position = GeoPoint(pos.latitude, pos.longitude)
                        title = "Base Station"
                    },
                )
            }
            plane?.position?.let { pos ->
                mapView.overlays.add(
                    Marker(mapView).apply {
                        position = GeoPoint(pos.latitude, pos.longitude)
                        title = "Plane"
                        rotation = plane.heading.toFloat()
                    },
                )
            }

            mapView.invalidate()
        }
    }
}

private suspend fun fetchRouteSuspending(repository: FlightRepository): List<Coordinate> =
    suspendCancellableCoroutine { continuation ->
        repository.fetchRoute { route -> continuation.resume(route) }
    }

package de.universegame.auto_flight.app

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import org.osmdroid.events.MapEventsReceiver
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.overlay.MapEventsOverlay
import org.osmdroid.views.overlay.Marker
import org.osmdroid.views.overlay.Polygon

@Composable
fun AreaScreen(
    viewModel: AppViewModel,
    onNext: () -> Unit,
) {
    LaunchedEffect(Unit) {
        viewModel.wizardStep = ConfigurationState.AREA_SELECTION
        viewModel.repository.fetchArea { polygon ->
            if (!polygon.isNullOrEmpty()) viewModel.areaPolygon = polygon
        }
    }

    var submitting by remember { mutableStateOf(false) }
    val state = viewModel.state
    val polygon = viewModel.areaPolygon

    Column(modifier = Modifier.fillMaxSize()) {
        Text(
            "Route Planner",
            style = MaterialTheme.typography.titleLarge,
            modifier = Modifier.padding(16.dp),
        )
        Text(
            "Tap the map to draw a polygon for the survey area.",
            modifier = Modifier.padding(horizontal = 16.dp),
        )

        Box(modifier = Modifier.weight(1f).fillMaxWidth()) {
            OsmMapView(
                onCreate = { mapView ->
                    val eventsOverlay = MapEventsOverlay(object : MapEventsReceiver {
                        override fun singleTapConfirmedHelper(p: GeoPoint): Boolean {
                            viewModel.areaPolygon = viewModel.areaPolygon + Coordinate(p.latitude, p.longitude)
                            return true
                        }

                        override fun longPressHelper(p: GeoPoint): Boolean = false
                    })
                    mapView.overlays.add(0, eventsOverlay)
                },
            ) { mapView ->
                mapView.overlays.removeAll { it is Polygon || it is Marker }

                if (polygon.size >= 3) {
                    val overlayPolygon = Polygon(mapView).apply {
                        points = polygon.map { GeoPoint(it.latitude, it.longitude) }
                        fillColor = 0x332563EB
                        strokeColor = android.graphics.Color.parseColor("#2563EB")
                        strokeWidth = 4f
                    }
                    mapView.overlays.add(overlayPolygon)
                } else if (polygon.isNotEmpty()) {
                    // Not a closed shape yet; still show the points being placed.
                    polygon.forEach { coordinate ->
                        mapView.overlays.add(
                            Marker(mapView).apply {
                                position = GeoPoint(coordinate.latitude, coordinate.longitude)
                            },
                        )
                    }
                }

                state.basePosition?.let { pos ->
                    mapView.overlays.add(
                        Marker(mapView).apply {
                            position = GeoPoint(pos.latitude, pos.longitude)
                            title = "Base Station"
                        },
                    )
                }
                state.planes[DEFAULT_PLANE_ID]?.position?.let { pos ->
                    mapView.overlays.add(
                        Marker(mapView).apply {
                            position = GeoPoint(pos.latitude, pos.longitude)
                            title = "Plane"
                        },
                    )
                }

                mapView.invalidate()
            }

            if (submitting) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(Color.Black.copy(alpha = 0.35f)),
                    contentAlignment = Alignment.Center,
                ) {
                    CircularProgressIndicator()
                }
            }
        }

        Column(modifier = Modifier.padding(16.dp)) {
            Button(
                onClick = {
                    if (polygon.size < 3 || submitting) return@Button
                    submitting = true
                    viewModel.repository.submitArea(polygon) { result ->
                        submitting = false
                        if (result == AreaSubmitResult.ACCEPTED) onNext()
                    }
                },
                enabled = polygon.size >= 3 && !submitting,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text("Next Step")
            }
            Text(
                "Points: ${polygon.size}" + if (polygon.isNotEmpty()) " (tap to add more, need at least 3)" else "",
                style = MaterialTheme.typography.bodySmall,
            )
        }
    }
}

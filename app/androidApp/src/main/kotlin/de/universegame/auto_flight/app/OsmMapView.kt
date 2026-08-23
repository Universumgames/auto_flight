package de.universegame.auto_flight.app

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import org.osmdroid.tileprovider.tilesource.TileSourceFactory
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView

/** Default center used when neither the base station nor the plane has reported a position yet. */
val DEFAULT_MAP_CENTER: GeoPoint = GeoPoint(51.316310347903176, 6.569530261539499)
const val DEFAULT_MAP_ZOOM = 15.0

/**
 * Hosts a single osmdroid [MapView] for the lifetime of the composable, wiring it
 * through the Activity lifecycle (onResume/onPause/onDetach) as osmdroid expects,
 * and handing the live instance to [content] on every recomposition so callers can
 * imperatively add/replace overlays.
 */
@Composable
fun OsmMapView(
    modifier: Modifier = Modifier,
    onCreate: (MapView) -> Unit = {},
    content: (MapView) -> Unit,
) {
    val lifecycleOwner = LocalLifecycleOwner.current
    var mapViewRef by remember { mutableStateOf<MapView?>(null) }

    AndroidView(
        modifier = modifier.fillMaxSize(),
        factory = { context ->
            MapView(context).apply {
                setTileSource(TileSourceFactory.MAPNIK)
                setMultiTouchControls(true)
                controller.setZoom(DEFAULT_MAP_ZOOM)
                controller.setCenter(DEFAULT_MAP_CENTER)
                onCreate(this)
                mapViewRef = this
            }
        },
        update = { mapView -> content(mapView) },
        onRelease = { mapView -> mapView.onDetach() },
    )

    DisposableEffect(lifecycleOwner, mapViewRef) {
        val current = mapViewRef
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_RESUME -> current?.onResume()
                Lifecycle.Event.ON_PAUSE -> current?.onPause()
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
    }
}

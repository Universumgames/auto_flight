package de.universegame.auto_flight.app

import android.app.Application
import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.neverEqualPolicy
import androidx.compose.runtime.setValue
import androidx.lifecycle.AndroidViewModel

private const val PREFS_NAME = "auto_flight_prefs"
private const val PREF_HOST = "host"

/**
 * Single source of truth for the app: owns the [FlightRepository], bridges its
 * callback-based updates into Compose state, and tracks the local wizard step
 * (which screen of the connect -> plan -> fly flow is active) plus the persisted
 * base-station host.
 */
class AppViewModel(application: Application) : AndroidViewModel(application) {

    val repository: FlightRepository = KtorFlightRepository()

    // AppState is mutated in place and re-delivered as the same reference on every
    // update, so the default structural-equality policy would treat it as unchanged
    // and skip recomposition - neverEqualPolicy() forces a recomposition every time.
    var state by mutableStateOf(repository.currentState(), neverEqualPolicy())
        private set

    var wizardStep by mutableStateOf(ConfigurationState.CONNECTION)

    /** Polygon vertices being drawn on the area-selection screen. */
    var areaPolygon by mutableStateOf(emptyList<Coordinate>())

    private val prefs = application.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    var host by mutableStateOf(prefs.getString(PREF_HOST, "") ?: "")

    private val listener = StateListener { newState -> state = newState }

    init {
        repository.addListener(listener)
    }

    fun connect() {
        prefs.edit().putString(PREF_HOST, host).apply()
        repository.connect(host)
    }

    override fun onCleared() {
        repository.removeListener(listener)
        repository.disconnect()
        super.onCleared()
    }
}

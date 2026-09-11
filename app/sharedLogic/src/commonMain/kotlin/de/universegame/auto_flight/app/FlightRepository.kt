package de.universegame.auto_flight.app

/** Outcome of submitting a planned survey area to the base station. */
enum class AreaSubmitResult {
    ACCEPTED,
    MISMATCH,
    FAILED,
}

// StateListener and the FlightRepository interface live in androidMain
// (sharedLogic/src/androidMain/.../FlightRepository.kt), not here, because they're
// built around the androidMain-only AppState (see Models.kt's package-level note).
// The iOS app talks to the base station through its own native `ConnectionManager`
// instead of this interface.

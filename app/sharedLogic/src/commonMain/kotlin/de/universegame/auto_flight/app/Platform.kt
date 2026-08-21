package de.universegame.auto_flight.app

interface Platform {
    val name: String
}

expect fun getPlatform(): Platform
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.swiftexport.ExperimentalSwiftExportDsl

plugins {
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.androidMultiplatformLibrary)
    alias(libs.plugins.kotlinSerialization)
}

// BleProtocol.generated.kt is generated at build time from the BLE wire contract in
// shared_components/flight_com (same generator CMake uses for the C++ header and Xcode for the Swift file).
// Hooked into commonMain below via srcDir(task), so every Kotlin compilation depends on it.
val flightComDir = layout.projectDirectory.dir("../../shared_components/flight_com")
val generatedBleProtocolDir = layout.buildDirectory.dir("generated/bleProtocol/kotlin")
val generateBleProtocol = tasks.register<Exec>("generateBleProtocol") {
    group = "build"
    description = "Generates BleProtocol.generated.kt from ble_protocol.json"
    val script = flightComDir.file("gen_ble_protocol.py")
    val schema = flightComDir.file("ble_protocol.json")
    inputs.file(script)
    inputs.file(schema)
    // ble_protocol.json mirrors the packet types declared in this schema (its "sourceSchema").
    inputs.file(flightComDir.file("packets.jtd.json"))
    outputs.dir(generatedBleProtocolDir)
    val outDir = generatedBleProtocolDir.get().asFile
    val outFile = outDir.resolve("de/universegame/auto_flight/app/wire/BleProtocol.generated.kt")
    doFirst { outDir.deleteRecursively() } // drop stale output, e.g. after a package move
    commandLine(
        "python3", script.asFile.absolutePath,
        "--schema", schema.asFile.absolutePath,
        "--out-kotlin", outFile.absolutePath,
    )
}

// FrontendPackets.generated.kt (public packet types + CBOR codec, the API Swift and Android
// use) and FrontendPacketsWire.generated.kt (their internal @Serializable wire twins) are
// generated from shared_components/flight_com/packets.jtd.json - the same schema CMake
// generates the C++ packet headers from. Hooked into commonMain below like generateBleProtocol.
val generatedPacketsDir = layout.buildDirectory.dir("generated/packets/kotlin")
val generatePackets = tasks.register<Exec>("generatePackets") {
    group = "build"
    description = "Generates FrontendPackets*.generated.kt from packets.jtd.json"
    val script = flightComDir.file("gen_packets.py")
    val schema = flightComDir.file("packets.jtd.json")
    inputs.file(script)
    inputs.file(schema)
    outputs.dir(generatedPacketsDir)
    val outDir = generatedPacketsDir.get().asFile
    doFirst { outDir.deleteRecursively() } // drop stale output, e.g. after a package move
    commandLine(
        "python3", script.asFile.absolutePath,
        "--schema", schema.asFile.absolutePath,
        "--out-kotlin", outDir.resolve("de/universegame/auto_flight/app/wire").absolutePath,
    )
}

kotlin {
    //listOf(
        iosArm64()//,
        iosSimulatorArm64()
    /*).forEach { iosTarget ->
        iosTarget.binaries.framework {
            baseName = "SharedLogic"
            isStatic = true
        }
    }*/

    @OptIn(ExperimentalSwiftExportDsl::class)
    swiftExport {
        // Set the root module name
        moduleName = "SharedLogic"

        // Set the collapse rule
        // Removes package prefix from generated Swift code
        flattenPackage = "de.universegame.auto_flight.app"

        // Configure external modules export
        /*@OptIn(ExperimentalSwiftExportDsl::class)
        export(project(":subproject")) {
            // Set the name for the exported module
            moduleName = "Subproject"
            // Set the collapse rule for the exported dependency
            flattenPackage = "de.universegame.auto_flight.library"
        }*/

    }

    android {
        namespace = "de.universegame.auto_flight.app.sharedLogic"
        compileSdk = libs.versions.android.compileSdk.get().toInt()
        minSdk = libs.versions.android.minSdk.get().toInt()

        compilerOptions {
            jvmTarget = JvmTarget.JVM_11
        }
        androidResources {
            enable = true
        }
        withHostTest {
            isIncludeAndroidResources = true
        }
    }

    sourceSets {
        // Models, packet parsing and the FlightRepository *interface* are shared. The
        // Ktor-based implementation (KtorFlightRepository) is deliberately Android-only:
        // Kotlin's Swift Export (still experimental/Alpha) unconditionally generates a
        // coroutine-bridging support shim for every swift-exported module, and that shim
        // does not compile once Ktor is part of the exported target's dependency graph
        // (verified via real `xcodebuild` runs against this project - kotlinx-coroutines-core
        // 1.11.0 fails to export `kotlinx.coroutines.selects.OnCancellationConstructor`,
        // and every other tested version's shim calls a `CoroutineDispatcher + CoroutineDispatcher`
        // overload that's a hard compile error in that same range). The iOS app therefore
        // implements FlightRepository natively in Swift (URLSession) against these shared
        // types instead. Revisit this split whenever the Swift Export toolchain matures.
        commonMain {
            kotlin.srcDir(generateBleProtocol)
            kotlin.srcDir(generatePackets)
        }
        commonMain.dependencies {
            // Only referenced by the `internal` wire-mirror types in the generated
            // FrontendPacketsWire.generated.kt (see the comment in FrontendPackets.generated.kt
            // on why the public packet types themselves stay unannotated).
            implementation(libs.kotlinx.serialization.cbor)
        }
        commonTest.dependencies {
            implementation(libs.kotlin.test)
        }
        androidMain.dependencies {
            implementation(libs.kotlinx.coroutines.core)
            implementation(libs.kotlinx.coroutines.android)
            implementation(libs.ktor.client.core)
            implementation(libs.ktor.client.websockets)
            implementation(libs.ktor.client.okhttp)
            // Only used for manual JsonObject/JsonArray building in KtorFlightRepository - kept
            // out of commonMain so kotlinx-serialization-core never enters the iOS/Swift Export
            // dependency graph (see the wire-types comment in FrontendPackets.generated.kt).
            implementation(libs.kotlinx.serialization.json)
        }
        iosMain.dependencies {
            // Not used by any of our own code - present solely because Swift Export's
            // generated `KotlinCoroutineSupport` shim unconditionally references
            // kotlinx-coroutines-core and fails to build without it on the classpath.
            // 1.9.0 is the newest version verified to export cleanly by itself; do not
            // bump without re-running `xcodebuild` against app/iosApp to confirm it still
            // works (see the comment above).
            implementation("org.jetbrains.kotlinx:kotlinx-coroutines-core:1.9.0")
        }
    }
}

import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.swiftexport.ExperimentalSwiftExportDsl

plugins {
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.androidMultiplatformLibrary)
    alias(libs.plugins.kotlinSerialization)
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
        commonMain.dependencies {
            // Only referenced by FrontendPackets.kt's `private` wire-mirror types (see the
            // comment there on why the public wire types themselves stay unannotated).
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
            // dependency graph (see the wire-types comment in FrontendPackets.kt).
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

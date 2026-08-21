import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import org.jetbrains.kotlin.gradle.swiftexport.ExperimentalSwiftExportDsl

plugins {
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.androidMultiplatformLibrary)
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
        commonMain.dependencies {
            // put your Multiplatform dependencies here
        }
        commonTest.dependencies {
            implementation(libs.kotlin.test)
        }
    }
}
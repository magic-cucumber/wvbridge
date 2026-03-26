@file:OptIn(org.jetbrains.kotlin.gradle.ExperimentalWasmDsl::class)

import com.android.build.api.dsl.KotlinMultiplatformAndroidLibraryTarget
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.kotlin.multiplatform.library")
    id("org.jetbrains.compose")
    id("org.jetbrains.kotlin.plugin.compose")
}

kotlin {
    jvm {
        compilerOptions { jvmTarget = JvmTarget.JVM_17 }
    }
    iosArm64()
    iosSimulatorArm64()

    sourceSets {
        commonMain.dependencies {
            api("org.jetbrains.compose.runtime:runtime:1.10.1")
            api("org.jetbrains.compose.ui:ui:1.10.1")
            api("org.jetbrains.compose.foundation:foundation:1.10.1")
            api("org.jetbrains.compose.material3:material3:1.10.0-alpha05")
            implementation(project(":core"))
        }

        jvmMain.dependencies {
            implementation(project(":platform:platform-${hostTarget.name.lowercase()}"))
        }
    }

    targets.withType<org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget>().configureEach {
        binaries.framework {
            baseName = "ComposeApp"
            isStatic = true
        }
    }
    extensions.configure<KotlinMultiplatformAndroidLibraryTarget>("android") {
        namespace = "top.kagg886.wvbridge.sharedUI"
        compileSdk = 36
        minSdk = 23
    }
}

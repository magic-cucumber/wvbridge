@file:OptIn(ExperimentalWasmDsl::class)

import com.vanniktech.maven.publish.KotlinMultiplatform
import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.kotlin.multiplatform.library")
    id("com.vanniktech.maven.publish")
    id("org.jetbrains.compose")
    id("org.jetbrains.kotlin.plugin.compose")

    id("org.jetbrains.dokka")
}

group = "top.kagg886.wvbridge"
version()

library(
    ios = {
        compilations.all {
            cinterops {
                val protocol by creating {
                    defFile("src/iosMain/interop/protocol.def")
                    packageName("top.kagg886.wvbridge.internal")
                    includeDirs("src/iosMain/interop/include")
                }
            }
        }
    }
) {
    sourceSets {
        commonMain.dependencies {
            implementation(libs.runtime)
            implementation(libs.ui)
            implementation(libs.foundation)
        }

        commonTest.dependencies {
            implementation(kotlin("test"))
        }
    }

    //https://kotlinlang.org/docs/native-objc-interop.html#export-of-kdoc-comments-to-generated-objective-c-headers
    targets.withType<org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget> {
        compilations["main"].compileTaskProvider.configure {
            compilerOptions {
                freeCompilerArgs.add("-Xexport-kdoc")
            }
        }
    }
}

publishing(KotlinMultiplatform())

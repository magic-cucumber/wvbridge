import com.android.build.api.dsl.KotlinMultiplatformAndroidLibraryExtension
import com.android.build.api.dsl.KotlinMultiplatformAndroidLibraryTarget
import org.gradle.api.Project
import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi
import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl
import org.jetbrains.kotlin.gradle.dsl.KotlinMultiplatformExtension
import org.jetbrains.kotlin.gradle.plugin.KotlinPlatformType
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinAndroidTarget
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget
import org.jetbrains.kotlin.gradle.targets.js.dsl.KotlinJsTargetDsl
import org.jetbrains.kotlin.gradle.targets.js.dsl.KotlinWasmTargetDsl
import org.jetbrains.kotlin.gradle.targets.jvm.KotlinJvmTarget

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2025/8/25 14:17
 * ================================================
 */

@OptIn(ExperimentalKotlinGradlePluginApi::class, ExperimentalWasmDsl::class)
fun Project.library(
    jvm: KotlinJvmTarget.() -> Unit = {},
    android: KotlinMultiplatformAndroidLibraryExtension.() -> Unit = {},
    ios: KotlinNativeTarget.() -> Unit = {},
//    wasm: KotlinWasmTargetDsl.() -> Unit = {},
    js: KotlinJsTargetDsl.() -> Unit = {},
    block: KotlinMultiplatformExtension.() -> Unit = {}
) {
    extensions.configure<KotlinMultiplatformExtension>("kotlin") {
        explicitApi()

        applyDefaultHierarchyTemplate {
            common {
                group("web") {
                    withJs()
                    withWasmJs()
                }
                group("jvmAndAndroid") {
                    withCompilations {
                        //compilation 'main' (com.android.build.api.variant.impl.KotlinMultiplatformAndroidLibraryTargetImpl@3252cea1)
                        it.target is KotlinMultiplatformAndroidLibraryTarget
                    }
                    withJvm()
                }
            }
        }
        jvmToolchain(17)
        jvm(jvm)

//        androidTarget()
        extensions.configure<KotlinMultiplatformAndroidLibraryTarget>("android") {
            namespace = group.toString()
            compileSdk = 35
            minSdk = 28
        }


//        android {
//            namespace = group.toString()
//            compileSdk = 35
//            minSdk = 28
//        }

        iosArm64(ios)
        iosSimulatorArm64(ios)
//        wasmJs { browser(); wasm() }
//        js { browser(); js() }

        block()
    }

}

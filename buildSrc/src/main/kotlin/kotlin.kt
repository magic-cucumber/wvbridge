import com.android.build.api.dsl.KotlinMultiplatformAndroidLibraryTarget
import com.android.build.api.dsl.androidLibrary
import com.android.build.gradle.LibraryExtension
import org.gradle.api.Project
import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi
import org.jetbrains.kotlin.gradle.ExperimentalWasmDsl
import org.jetbrains.kotlin.gradle.dsl.KotlinMultiplatformExtension
import org.jetbrains.kotlin.gradle.plugin.KotlinPlatformType

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2025/8/25 14:17
 * ================================================
 */

@OptIn(ExperimentalKotlinGradlePluginApi::class, ExperimentalWasmDsl::class)
fun Project.library(
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
        jvm()
        androidLibrary {
            namespace = group.toString()
            compileSdk = 35
            minSdk = 28
        }

//        iosArm64()
//        iosSimulatorArm64()
//        wasmJs { browser() }
//        js { browser() }

        block()
    }

}

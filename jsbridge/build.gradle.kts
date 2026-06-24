import com.vanniktech.maven.publish.KotlinMultiplatform

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("com.android.kotlin.multiplatform.library")
    id("com.vanniktech.maven.publish")

    id("org.jetbrains.dokka")
}

group = "top.kagg886.wvbridge"
version()

library {
    sourceSets {
        commonMain.dependencies {
            api(project(":core"))
        }

        commonTest.dependencies {
            implementation(kotlin("test"))
        }
    }
}

publishing(KotlinMultiplatform())

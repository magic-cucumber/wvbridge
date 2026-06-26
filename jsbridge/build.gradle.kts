import com.vanniktech.maven.publish.KotlinMultiplatform

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("org.jetbrains.kotlin.plugin.serialization")
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
            api(libs.kotlinx.serialization.json)
            implementation(libs.kotlinx.coroutines.core)
        }

        commonTest.dependencies {
            implementation(kotlin("test"))
        }
    }
}

publishing(KotlinMultiplatform())

dokka {
    moduleName.set("wvbridge:jsbridge")
    pluginsConfiguration {
        versioning {
            this.version.set(project.version.toString())
        }
    }

    dokkaSourceSets.configureEach {
        includes.from("Module.md")
    }
}


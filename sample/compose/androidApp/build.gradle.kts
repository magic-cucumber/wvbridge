plugins {
    id("org.jetbrains.kotlin.plugin.compose")
    id("com.android.application")
}

android {
    namespace = "sample.app"
    compileSdk = 36

    defaultConfig {
        minSdk = 23
        targetSdk = 36

        applicationId = "sample.app"
        versionCode = 1
        versionName = "1.0.0"
    }
    
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        compose = true
    }
}

dependencies {
    implementation(project(":sample:compose:sharedUI"))
    implementation("androidx.activity:activity-compose:1.10.1")
}

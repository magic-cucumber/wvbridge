plugins {
    `kotlin-dsl`
}

repositories {
    mavenCentral()
    google()
    gradlePluginPortal()
}

dependencies {
    api(gradleApi())
    api(gradleKotlinDsl())

    api(libs.kotlin.gradle.plugin) {
        exclude("org.jetbrains.kotlin", "kotlin-stdlib")
        exclude("org.jetbrains.kotlin", "kotlin-stdlib-common")
        exclude("org.jetbrains.kotlin", "kotlin-reflect")
    }

    api(libs.android.gradle.plugin)
    api(libs.maven.publish.gradle.plugin)
    api(libs.compose.gradle.plugin)
    api(libs.compose.compiler.gradle.plugin)
}

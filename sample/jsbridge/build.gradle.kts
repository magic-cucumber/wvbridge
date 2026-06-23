plugins {
    id("org.jetbrains.kotlin.jvm")
    application
}

group = "top.kagg886.wvbridge.demo"
version = "1.0"

repositories {
    mavenCentral()
    google()
}

dependencies {
    implementation(project(":core"))
    implementation(project(":platform:platform-${hostTarget.name.lowercase()}"))
    implementation(libs.kotlinx.coroutines.core)
    testImplementation(kotlin("test"))
}

kotlin {
    jvmToolchain(17)
}

tasks.test {
    useJUnitPlatform()
}

application {
    mainClass.set("MainKt")
}

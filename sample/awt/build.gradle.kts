plugins {
    id("org.jetbrains.kotlin.jvm")
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
    testImplementation(kotlin("test"))
}

kotlin {
    jvmToolchain(17)
}

tasks.test {
    useJUnitPlatform()
}

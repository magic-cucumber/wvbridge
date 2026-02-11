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
    testImplementation(kotlin("test"))
}

kotlin {
    jvmToolchain(17)
}

tasks.test {
    useJUnitPlatform()
}

application {
    mainClass.set("MainKt") // 注意：文件名是 main.kt，类名通常是 MainKt
}

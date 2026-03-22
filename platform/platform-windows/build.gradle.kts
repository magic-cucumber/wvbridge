import com.vanniktech.maven.publish.KotlinJvm
import org.gradle.internal.jvm.Jvm

plugins {
    id("org.jetbrains.kotlin.jvm")
    id("com.vanniktech.maven.publish")
}

group = "top.kagg886.wvbridge"
version()

kotlin {
    jvmToolchain(17)
}

val processBuild = tasks.register<Exec>("processBuild") {
    onlyIf {
        System.getProperty("os.name").startsWith("Win")
    }
    workingDir = project.file("native")
    environment("JAVA_HOME", Jvm.current().javaHome.absolutePath)
    commandLine(
        "powershell", "-NoProfile", "-Command",
        $$"""
            cmake -S . -B build;
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            cmake --build build --config Debug;
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        """.trimIndent()
    )
}

tasks.named<ProcessResources>("processResources") {
    dependsOn(processBuild)
    from(project.file("native/build/wvbridge.dll"))
}

publishing(KotlinJvm())

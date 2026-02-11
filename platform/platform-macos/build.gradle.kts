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
        System.getProperty("os.name").startsWith("Mac")
    }
    workingDir = project.file("native")
    environment("JAVA_HOME", Jvm.current().javaHome.absolutePath)
    commandLine(
        "zsh", "-c",
        """
            mkdir -p build && \
            cd build && \
            cmake .. && \
            make &&\
            echo $(shasum -a 256 lib/libwvbridge.dylib | cut -d ' ' -f 1) > build-macos.hash
        """.trimIndent()
    )
}

// Configure JVM processResources task
tasks.named<ProcessResources>("processResources") {
    dependsOn(processBuild)
    from(project.file("native/build/lib/libwvbridge.dylib"))
    from(project.file("native/build/build-macos.hash"))
}


publishing(KotlinJvm())

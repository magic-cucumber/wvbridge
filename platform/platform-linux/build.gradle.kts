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
        System.getProperty("os.name").startsWith("Linux")
    }
    workingDir = project.file("native")
    environment("JAVA_HOME", Jvm.current().javaHome.absolutePath)
    commandLine(
        "bash", "-c",
        """
            mkdir -p build && \
            cd build && \
            cmake .. && \
            make &&\
            echo $(shasum -a 256 lib/libwvbridge.so | cut -d ' ' -f 1) > build-linux.hash
        """.trimIndent()
    )
}

// Configure JVM processResources task
tasks.named<ProcessResources>("processResources") {
    dependsOn(processBuild)
    from(project.file("native/build/lib/libwvbridge.so"))
    from(project.file("native/build/build-linux.hash"))
}


publishing(KotlinJvm())

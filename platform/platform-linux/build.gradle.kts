import com.vanniktech.maven.publish.KotlinJvm
import org.gradle.jvm.toolchain.JavaLanguageVersion
import org.gradle.jvm.toolchain.JavaToolchainService
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("org.jetbrains.kotlin.jvm")
    id("com.vanniktech.maven.publish")
}

group = "top.kagg886.wvbridge"
version()

kotlin {
    jvmToolchain(17)
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

val java17Launcher = extensions.getByType<JavaToolchainService>().launcherFor {
    languageVersion.set(JavaLanguageVersion.of(17))
}

val nativeBuildType = nativeBuildType()

val processBuild = tasks.register<Exec>("processBuild") {
    inputs.property("nativeBuildType", nativeBuildType)
    onlyIf {
        System.getProperty("os.name").startsWith("Linux")
    }
    workingDir = project.file("native")
    environment("JAVA_HOME", java17Launcher.get().metadata.installationPath.asFile.absolutePath)
    commandLine(
        "bash", "-c",
        """
            cmake -S . -B build/$nativeBuildType -DCMAKE_BUILD_TYPE=$nativeBuildType && \
            cmake --build build/$nativeBuildType --config $nativeBuildType
        """.trimIndent()
    )
}

// Configure JVM processResources task
tasks.named<ProcessResources>("processResources") {
    dependsOn(processBuild)
    from(project.file("native/build/$nativeBuildType/lib/libwvbridge.so"))
}


publishing(KotlinJvm())

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

/**
 * Windows 原生库支持多架构：x64 / ARM64。
 *
 * 仅为 host 构建服务：根据当前机器的 os.arch 自动选择 msbuild Platform，不支持覆盖。
 */
val msbuildPlatform: String = when (System.getProperty("os.arch").lowercase()) {
    "amd64", "x86_64" -> "x64"
    "aarch64", "arm64" -> "ARM64"
    else -> error("Unsupported Windows arch for native build: ${System.getProperty("os.arch")}")
}

val processBuild = tasks.register<Exec>("processBuild") {
    onlyIf {
        System.getProperty("os.name").startsWith("Win")
    }
    workingDir = project.file("native")

    // 为 vcxproj 提供 JNI include/lib 查找路径
    environment("JAVA_HOME", Jvm.current().javaHome.absolutePath)

    commandLine(
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        $$"""
            Write-Host "[wvbridge] msbuild platform = $platform"

            msbuild 'wvbridge.sln' '/restore' "/p:Configuration=Release" "/p:Platform=$$msbuildPlatform" '/m'

            # Windows 下仅输出到 build\\wvbridge.dll
            $dll1 = 'build\\wvbridge.dll'
            if (!(Test-Path $dll1)) {
                throw "wvbridge.dll not found at $dll1"
            }

            $hash = (Get-FileHash $dll1 -Algorithm SHA256).Hash.ToLower()
            Set-Content -Path 'build\\build-windows.hash' -Value $hash -Encoding ascii -NoNewline
        """.trimIndent()
    )
}

// Configure JVM processResources task
tasks.named<ProcessResources>("processResources") {
    dependsOn(processBuild)
    from(project.file("native/build/wvbridge.dll"))
    from(project.file("native/build/build-windows.hash"))
}


publishing(KotlinJvm())

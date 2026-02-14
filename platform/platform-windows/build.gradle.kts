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

    // 为 vcxproj 提供 JNI include/lib 查找路径
    environment("JAVA_HOME", Jvm.current().javaHome.absolutePath)

    // 使用 msbuild 构建（NuGet 还原 + 编译），输出到 native/build/lib/wvbridge.dll
    // PowerShell：
    // 1) 自动通过 vswhere 定位 MSBuild.exe（若可用）
    // 2) 执行 /t:Restore;Build 以还原 NuGet(WebView2) 并构建
    // 3) 生成单行 SHA256（与其它平台 hash 文件一致语义）
    commandLine(
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        """
            msbuild 'wvbridge.sln' '/p:Configuration=Release' '/p:Platform=x64' '/m'
            ${'$'}hash = (Get-FileHash 'build\\wvbridge.dll' -Algorithm SHA256).Hash.ToLower()
            Set-Content -Path 'build\\build-windows.hash' -Value ${'$'}hash -Encoding ascii -NoNewline
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

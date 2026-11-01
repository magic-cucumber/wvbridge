import org.gradle.api.Project

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2025/8/25 14:28
 * ================================================
 */

enum class JvmTarget {
    MACOS,
    WINDOWS,
    LINUX;
}

fun Project.nativeBuildType(): String {
    val invocationDir = gradle.startParameter.projectDir ?: gradle.startParameter.currentDir
    val relativeProjectPath = invocationDir.relativeTo(rootDir).invariantSeparatorsPath
        .split('/').filter { it.isNotEmpty() && it != "." }.joinToString(":")
    val requestedTasks = gradle.startParameter.taskNames.map { task ->
        if (task.startsWith(":")) task else ":$relativeProjectPath:$task".replace("::", ":")
    }
    // Mixed sample/library invocations use Release so published libraries stay release builds.
    return if (requestedTasks.isNotEmpty() && requestedTasks.all {
            it.startsWith(":sample:") && !it.substringAfterLast(':').startsWith("publish", ignoreCase = true)
        }) "Debug" else "Release"
}

val hostTarget by lazy {
    val osName = System.getProperty("os.name")
    when {
        osName.startsWith("Mac") -> JvmTarget.MACOS
        osName.startsWith("Win") -> JvmTarget.WINDOWS
        osName.startsWith("Linux") -> JvmTarget.LINUX
        else -> error("Unsupported OS: $osName")
    }
}

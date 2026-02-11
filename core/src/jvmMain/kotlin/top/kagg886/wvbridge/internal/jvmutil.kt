package top.kagg886.wvbridge.internal

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/2/11 09:47
 * ================================================
 */

internal val jvmTarget by lazy {
    val osName = System.getProperty("os.name")
    when {
        osName == "Mac OS X" -> JvmTarget.MACOS
        osName.startsWith("Win") -> JvmTarget.WINDOWS
        osName.startsWith("Linux") -> JvmTarget.LINUX
        else -> error("Unsupported OS: $osName")
    }
}

internal enum class JvmTarget {
    MACOS,
    WINDOWS,
    LINUX;
}

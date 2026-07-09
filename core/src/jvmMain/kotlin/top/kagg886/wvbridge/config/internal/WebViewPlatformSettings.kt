package top.kagg886.wvbridge.config.internal

internal data class NativeWindowsWebViewPlatformSetting(
    val userAgent: String?,
    val dataDir: String
)

internal data class NativeLinuxWebViewPlatformSetting(
    val userAgent: String?,
    val dataDir: String,
    val cacheDir: String
)

internal data class NativeMacOSWebViewPlatformSetting(
    val userAgent: String?,
    val websiteDataStore: NativeMacOSWebViewWebsiteDataStore
)

internal enum class NativeMacOSWebViewWebsiteDataStore {
    DEFAULT,
    NON_PERSISTENT
}

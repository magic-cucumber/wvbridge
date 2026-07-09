package top.kagg886.wvbridge.config

import java.io.File
import top.kagg886.wvbridge.config.internal.NativeLinuxWebViewPlatformSetting
import top.kagg886.wvbridge.config.internal.NativeMacOSWebViewPlatformSetting
import top.kagg886.wvbridge.config.internal.NativeMacOSWebViewWebsiteDataStore
import top.kagg886.wvbridge.config.internal.NativeWindowsWebViewPlatformSetting
import top.kagg886.wvbridge.internal.JvmTarget
import top.kagg886.wvbridge.internal.jvmTarget

/**
 * JVM desktop WebView configuration.
 *
 * The JVM artifact contains settings for all desktop backends. At runtime, only
 * the setting matching the current operating system is passed to native code.
 *
 * @property windowSetting Settings used by the Windows WebView2 backend.
 * @property linuxSetting Settings used by the Linux WebKitGTK backend.
 * @property macOSSetting Settings used by the macOS WKWebView backend.
 */
public actual data class WebViewPlatformConfig(
    val windowSetting: Windows = Windows(),
    val linuxSetting: Linux = Linux(),
    val macOSSetting: MacOS = MacOS()
) {

    /**
     * Windows WebView2-specific settings for JVM desktop.
     *
     * @property dataDir WebView2 user data folder. This value is required by the
     * native backend and is passed to `CreateCoreWebView2EnvironmentWithOptions`.
     */
    public data class Windows(
        val dataDir: String = System.getProperty("java.io.tmpdir") + File.separator + "wvbridge"
    )

    /**
     * Linux WebKitGTK-specific settings for JVM desktop.
     *
     * @property dataDir Base website data directory used by WebKitGTK, or `null` to
     * use WebKitGTK's default location.
     * @property cacheDir Base website cache directory used by WebKitGTK, or `null`
     * to use WebKitGTK's default location.
     */
    public data class Linux(
        val dataDir: String = System.getProperty("java.io.tmpdir") + File.separator + "wvbridge" + File.separator + "data",
        val cacheDir: String = System.getProperty("java.io.tmpdir") + File.separator + "wvbridge" + File.separator + "cache"
    )

    /**
     * macOS WKWebView-specific settings for JVM desktop.
     *
     * @property websiteDataStore Controls whether WebKit uses the default persistent
     * website data store or a non-persistent one.
     */
    public data class MacOS(
        val websiteDataStore: WebsiteDataStore = WebsiteDataStore.DEFAULT
    ) {
        /**
         * Website data storage mode used by the JVM macOS WKWebView backend.
         */
        public enum class WebsiteDataStore {
            /**
             * Use WebKit's default persistent website data store.
             */
            DEFAULT,

            /**
             * Use a non-persistent website data store for the created WebView.
             */
            NON_PERSISTENT
        }
    }

}

/**
 * Returns the default JVM desktop WebView platform configuration.
 */
public actual fun defaultPlatformConfig(): WebViewPlatformConfig = WebViewPlatformConfig()

internal fun WebViewConfig.currentJvmPlatformSetting(): Any = when (jvmTarget) {
    JvmTarget.WINDOWS -> NativeWindowsWebViewPlatformSetting(
        userAgent = userAgent,
        dataDir = platform.windowSetting.dataDir
    )

    JvmTarget.LINUX -> NativeLinuxWebViewPlatformSetting(
        userAgent = userAgent,
        dataDir = platform.linuxSetting.dataDir,
        cacheDir = platform.linuxSetting.cacheDir
    )

    JvmTarget.MACOS -> NativeMacOSWebViewPlatformSetting(
        userAgent = userAgent,
        websiteDataStore = NativeMacOSWebViewWebsiteDataStore.valueOf(platform.macOSSetting.websiteDataStore.name)
    )
}

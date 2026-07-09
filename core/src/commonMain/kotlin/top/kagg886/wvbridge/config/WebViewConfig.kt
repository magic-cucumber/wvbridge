package top.kagg886.wvbridge.config

/**
 * Platform-independent configuration used when creating a native WebView.
 *
 * The values are applied during native WebView creation. Changing a remembered
 * [WebViewConfig] after a controller has been created does not reconfigure the
 * existing native instance; create a new controller to apply different settings.
 *
 * @property userAgent Custom user agent for the WebView instance, or `null` to
 * use the platform default. On JVM, this value overrides the user-agent value
 * inside the current JVM platform setting.
 * @property platform Platform-specific settings for the current target.
 */
public data class WebViewConfig(
    val userAgent: String? = null,
    val platform: WebViewPlatformConfig = defaultPlatformConfig()
)


/**
 * Platform-specific WebView configuration.
 *
 * The actual shape depends on the current target:
 * - Android: no platform-specific options.
 * - iOS: WebKit website data store selection.
 * - JVM: Windows, Linux, and macOS native backend settings.
 */
public expect class WebViewPlatformConfig

/**
 * Returns the default platform-specific WebView configuration for the current target.
 */
public expect fun defaultPlatformConfig(): WebViewPlatformConfig

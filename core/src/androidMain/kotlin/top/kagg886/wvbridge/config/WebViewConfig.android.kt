package top.kagg886.wvbridge.config

/**
 * Android platform configuration.
 *
 * Android currently has no additional platform-specific options. Use
 * [WebViewConfig.userAgent] to configure the instance user agent.
 */
public actual class WebViewPlatformConfig

/**
 * Returns the default Android WebView platform configuration.
 */
public actual fun defaultPlatformConfig(): WebViewPlatformConfig = WebViewPlatformConfig()

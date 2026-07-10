package top.kagg886.wvbridge.config

import androidx.webkit.Profile

/**
 * Android platform configuration.
 *
 * @property profile Optional AndroidX WebKit [Profile] to attach to the created
 * WebView. Use this when a WebView needs isolated website data, such as cookies,
 * DOM storage, permissions, service workers, and cache.
 *
 * Supplying a profile requires AndroidX WebKit multi-profile support
 * (`WebViewFeature.MULTI_PROFILE`) from the current Android WebView runtime.
 * Platforms that do not support multi-profile WebView should reject non-null
 * values.
 *
 * Leave this value `null` to use the platform default WebView profile.
 */
public actual class WebViewPlatformConfig(
    public val profile: Profile? = null
)

/**
 * Returns the default Android WebView platform configuration.
 */
public actual fun defaultPlatformConfig(): WebViewPlatformConfig = WebViewPlatformConfig()

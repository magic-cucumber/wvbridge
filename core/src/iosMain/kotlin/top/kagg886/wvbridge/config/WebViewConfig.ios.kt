package top.kagg886.wvbridge.config

/**
 * Website data storage mode used by iOS [platform.WebKit.WKWebView].
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

/**
 * iOS-specific WKWebView configuration.
 *
 * @property websiteDataStore Controls whether WebKit uses the default persistent
 * website data store or a non-persistent one.
 */
public actual data class WebViewPlatformConfig(
    val websiteDataStore: WebsiteDataStore = WebsiteDataStore.DEFAULT
)

/**
 * Returns the default iOS WebView platform configuration.
 */
public actual fun defaultPlatformConfig(): WebViewPlatformConfig = WebViewPlatformConfig()

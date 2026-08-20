package top.kagg886.wvbridge.cookie

import platform.Foundation.*
import platform.WebKit.WKHTTPCookieStore
import platform.darwin.DISPATCH_TIME_FOREVER
import platform.darwin.dispatch_semaphore_create
import platform.darwin.dispatch_semaphore_signal
import platform.darwin.dispatch_semaphore_wait

/**
 * iOS implementation of [CookieManager] backed by [WKHTTPCookieStore].
 *
 * The controller binds this wrapper to
 * `WKWebView.configuration.websiteDataStore.httpCookieStore`, so it follows the configured
 * `WKWebsiteDataStore` for that WebView. [get] converts [uri] to `NSURL`, reads all cookies from
 * the store, filters them by domain and path, and returns common [Cookie] objects containing each
 * matching cookie's name and value.
 *
 * `WKHTTPCookieStore.getAllCookies` is asynchronous. This wrapper waits for the native callback so
 * the common [CookieManager.get] contract can return a synchronous snapshot.
 */
public class WKWebViewCookieManager(override val impl: WKHTTPCookieStore) : CookieManager<WKHTTPCookieStore> {

    override fun get(uri: String): List<Cookie> {
        val url = NSURL.URLWithString(uri) ?: return emptyList()
        var result: List<Cookie> = emptyList()
        val semaphore = dispatch_semaphore_create(0)

        impl.getAllCookies { cookies ->
            result = cookies
                .orEmpty()
                .filterIsInstance<NSHTTPCookie>()
                .filter { it.matches(url) }
                .map { it.toPlatformCookie() }
            dispatch_semaphore_signal(semaphore)
        }

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER)
        return result
    }

    private fun NSHTTPCookie.matches(url: NSURL): Boolean {
        val host = url.host ?: return false
        val requestPath = url.path?.takeIf { it.isNotEmpty() } ?: "/"
        val cookieDomain = domain.trimStart('.')

        return (host == cookieDomain || host.endsWith(".$cookieDomain")) && requestPath.startsWith(path)
    }

    private fun NSHTTPCookie.toPlatformCookie(): PlatformCookie {
        return PlatformCookie(impl = this)
    }
}

public data class PlatformCookie(val impl: NSHTTPCookie) : Cookie {
    override val name: String
        get() = impl.name
    override val value: String
        get() = impl.value
}

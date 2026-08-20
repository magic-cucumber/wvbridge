package top.kagg886.wvbridge.cookie

import android.webkit.CookieManager as AndroidCookieManager

/**
 * Android implementation of [CookieManager].
 *
 * The controller creates this wrapper from AndroidX WebKit `Profile.cookieManager` when a WebView
 * profile is configured, otherwise it falls back to [android.webkit.CookieManager.getInstance].
 * [get] delegates to `CookieManager.getCookie(uri)` and parses the returned semicolon-separated
 * `name=value` pairs into common [Cookie] objects.
 *
 * Android's native API does not expose cookie attributes through `getCookie`, so this common wrapper
 * returns only [Cookie.name] and [Cookie.value]. Invalid or empty native entries are ignored.
 */
public class AndroidWebViewCookieManager(override val impl: AndroidCookieManager) : CookieManager<AndroidCookieManager> {

    override fun get(uri: String): Set<Cookie> {
        return impl.getCookie(uri)
            ?.split(';')
            .orEmpty()
            .mapNotNull { it.trim().toCookieOrNull() }
            .toSet()
    }

    private fun String.toCookieOrNull(): Cookie? {
        val separator = indexOf('=')
        if (separator <= 0) return null

        return SimpleCookie(
            name = substring(0, separator),
            value = substring(separator + 1),
        )
    }
}

private data class SimpleCookie(override val name: String, override val value: String) : Cookie

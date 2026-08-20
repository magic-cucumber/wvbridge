package top.kagg886.wvbridge.cookie

/**
 * Common view of a cookie visible to a WebView request.
 *
 * The common API intentionally exposes only [name] and [value]. Browser engines keep additional
 * cookie attributes such as domain, path, expiry, `HttpOnly`, `Secure`, session state, and
 * `SameSite`, but support for reading or mutating those attributes differs by platform and native
 * backend.
 *
 * Platform notes:
 *
 * - Android parses the `name=value; name2=value2` string returned by
 *   `android.webkit.CookieManager.getCookie`, so the common objects contain only [name] and
 *   [value].
 * - iOS maps matching `NSHTTPCookie` objects from `WKHTTPCookieStore` to [name] and [value].
 * - Desktop/JVM receives structured cookies from the native backend. Those objects carry extra
 *   properties internally, but the public [Cookie] contract remains [name] and [value].
 */
public interface Cookie {
    /**
     * Cookie name as reported by the native WebView cookie store.
     */
    public val name: String

    /**
     * Cookie value as reported by the native WebView cookie store.
     */
    public val value: String
}

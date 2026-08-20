package top.kagg886.wvbridge.cookie

/**
 * Reads cookies from the native cookie store used by a [top.kagg886.wvbridge.WebViewController].
 *
 * `CookieManager` is the common, read-only surface exposed by
 * [top.kagg886.wvbridge.WebViewController.cookies]. The concrete instance is created lazily by the
 * platform controller and is bound to the same native WebView/profile/data-store as navigation,
 * JavaScript, and interception APIs.
 *
 * Platform mapping:
 *
 * | Platform | Public implementation | Native store |
 * | --- | --- | --- |
 * | Android | [AndroidWebViewCookieManager] | `android.webkit.CookieManager`, or `Profile.cookieManager` when AndroidX WebKit multi-profile is configured |
 * | iOS | [WKWebViewCookieManager] | `WKWebView.configuration.websiteDataStore.httpCookieStore` |
 * | Desktop/JVM | [SwingPanelCookieManager] | `WebViewBridgePanel` native backend cookie manager |
 *
 * Desktop/JVM details:
 *
 * - The desktop wrapper exposes [WebViewPanelCookieManager] as [impl]. That platform-specific
 *   manager supports `putCookie`, `removeCookie`, `clearAll`, and `all` in addition to the common
 *   [get] operation.
 * - The desktop native backend is WebView2 on Windows, WebKitGTK on Linux, and WebKit on macOS.
 *   Calls are forwarded through JNI from `WebViewBridgePanel` to the matching native cookie
 *   manager.
 * - Desktop cookie mutation currently accepts only the internal structured cookie objects returned
 *   by the desktop native backend. Passing an arbitrary [Cookie] implementation to
 *   `WebViewPanelCookieManager.putCookie` or `removeCookie` throws [IllegalArgumentException].
 *
 * Behavior and limitations:
 *
 * - [get] returns cookies that the platform reports as applicable to [uri]. For Android this is
 *   the parsed result of `CookieManager.getCookie(uri)`. For iOS this implementation reads all
 *   cookies from `WKHTTPCookieStore` and filters by host/domain and request path. For desktop,
 *   WebView2 and WebKitGTK ask the native cookie manager for cookies for the URI; desktop macOS
 *   reads all cookies and filters with the shared domain/path/secure matching logic.
 * - The returned common [Cookie] only guarantees [Cookie.name] and [Cookie.value]. Platform
 *   implementations may carry more properties internally, but common code should not rely on
 *   them unless it deliberately casts to a platform-specific type.
 * - `uri` must be a URL understood by the underlying platform. Desktop native implementations
 *   require an absolute `http://` or `https://` URI with a host. Invalid desktop URIs throw
 *   [IllegalArgumentException]. Android and iOS follow their native URL parser behavior and may
 *   return an empty list for invalid input.
 * - This API observes the WebView cookie store; it does not parse `document.cookie` directly and
 *   does not expose response headers or subresource request cookies separately.
 * - Calls may touch native UI/runtime threads internally and should not be treated as pure in-memory
 *   lookups. In particular, iOS and desktop implementations bridge asynchronous native cookie APIs
 *   into a synchronous [get] result.
 *
 * @param Impl Native cookie manager type exposed for platform-specific operations.
 */
public interface CookieManager<out Impl> {
    /**
     * The platform-specific cookie manager backing this wrapper.
     *
     * Common code should prefer [get]. Use [impl] only when the caller intentionally needs native
     * behavior that is not part of the common API, such as Android profile cookie operations, direct
     * iOS `WKHTTPCookieStore` access, or desktop [WebViewPanelCookieManager] mutation helpers.
     */
    public val impl: Impl

    /**
     * Returns cookies that apply to [uri] in this WebView's native cookie store.
     *
     * The returned list is a snapshot. Later page loads, redirects, JavaScript `document.cookie`
     * writes, native cookie changes, or calls through [impl] are not reflected in a previously
     * returned list.
     *
     * @param uri Target request URI used by the platform to select matching cookies.
     * @return Cookies visible to a request for [uri].
     */
    public fun get(uri: String): Set<Cookie>
}

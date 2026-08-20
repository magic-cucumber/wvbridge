package top.kagg886.wvbridge.cookie

import top.kagg886.wvbridge.internal.cookie.StructedCookie
import top.kagg886.wvbridge.internal.cookie.WebViewPanelCookieManager as InternalWebViewPanelCookieManager

/**
 * Desktop/JVM implementation of [CookieManager] exposed from the Swing-backed WebView controller.
 *
 * This wrapper is created by `SwingPanelController` and delegates [get] to [WebViewPanelCookieManager.all].
 * The underlying `WebViewBridgePanel` forwards cookie operations through JNI to the active desktop
 * backend: WebView2 on Windows, WebKitGTK on Linux, and WebKit on macOS.
 */
public class SwingPanelCookieManager internal constructor(override val impl: WebViewPanelCookieManager) :
    CookieManager<WebViewPanelCookieManager> {
    override fun get(uri: String): List<Cookie> {
        return impl.all(uri)
    }
}

/**
 * Desktop-specific cookie operations backed by the internal native panel cookie store.
 *
 * This type is available through `controller.cookies.impl` on desktop/JVM. It exposes mutation
 * operations that are not part of the common [CookieManager] API because Android and iOS already
 * expose their native managers directly through their own [CookieManager.impl] types.
 *
 * Supported structured cookie properties on the desktop native bridge are:
 *
 * | Key | Meaning |
 * | --- | --- |
 * | `name` | Cookie name. Required and must not be empty. |
 * | `value` | Cookie value. Required. |
 * | `domain` | Cookie domain. Defaults to the host from the URI when omitted during mutation. |
 * | `path` | Cookie path. Defaults to the URI's default cookie path when omitted during mutation. |
 * | `expires` | Expiration timestamp in milliseconds since Unix epoch. |
 * | `httpOnly` | Boolean string: `true`, `1`, `yes`, `false`, `0`, or `no`. |
 * | `secure` | Boolean string with the same accepted values as `httpOnly`. |
 * | `session` | Boolean string. Defaults to `true` when `expires` is absent. |
 * | `sameSite` | `NONE`, `LAX`, or `STRICT` where supported by the native backend. |
 *
 * Mutation methods currently require cookie objects returned by this desktop backend. Passing an
 * arbitrary [Cookie] implementation throws [IllegalArgumentException] because the native bridge
 * needs the structured property map described above.
 */
public class WebViewPanelCookieManager internal constructor(internal val impl: InternalWebViewPanelCookieManager) {
    /**
     * Stores or updates [cookie] for [uri] in the desktop WebView cookie store.
     *
     * [uri] must be an absolute `http://` or `https://` URI with a host. When [cookie] does not
     * contain `domain` or `path`, the native bridge derives them from [uri].
     */
    public fun putCookie(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        impl.put(uri, cookie)
    }

    /**
     * Removes [cookie] from [uri] in the desktop WebView cookie store.
     *
     * [uri] is used to validate and normalize the cookie origin before delegating to WebView2,
     * WebKitGTK, or WebKit.
     */
    public fun removeCookie(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        impl.remove(uri, cookie)
    }

    /**
     * Removes all cookies from the desktop WebView cookie store.
     */
    public fun clearAll() {
        impl.clearAll()
    }

    /**
     * Returns all cookies that apply to [uri] in the desktop WebView cookie store.
     *
     * [uri] must be an absolute `http://` or `https://` URI with a host. The returned cookies are
     * structured desktop cookies, but callers should treat them as [Cookie] unless they explicitly
     * need desktop-only mutation support.
     */
    public fun all(uri: String): List<Cookie> = impl.all(uri)
}

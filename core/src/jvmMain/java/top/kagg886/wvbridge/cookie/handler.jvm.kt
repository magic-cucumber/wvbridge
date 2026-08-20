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
    override fun get(uri: String): Set<Cookie> {
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
     * contain `domain` or `path`, the native bridge derives them from [uri]. Existing cookies are
     * updated only when the native cookie key matches: `name`, `domain`, and `path`. Cookies with
     * the same name but a different domain or path can coexist.
     *
     * Platform notes:
     *
     * | Platform | Native operation | Key behavior |
     * | --- | --- | --- |
     * | Windows | WebView2 `AddOrUpdateCookie` | Adds or updates a cookie in the current WebView2 profile. |
     * | Linux | WebKitGTK `webkit_cookie_manager_add_cookie` | Adds through WebKitGTK/libsoup storage; matching cookie keys are replaced by the underlying store. |
     * | macOS | `WKHTTPCookieStore.setCookie` | Stores the cookie in the WebKit website data store for this WebView configuration. |
     */
    public fun putCookie(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        impl.put(uri, cookie)
    }

    /**
     * Removes [cookie] from [uri] in the desktop WebView cookie store.
     *
     * [uri] is used to validate and normalize the cookie origin before delegating to WebView2,
     * WebKitGTK, or WebKit. Removal targets the cookie identified by the structured cookie's
     * `name`, `domain`, and `path`; the `value` is not used as the identity.
     *
     * Platform notes:
     *
     * | Platform | Native operation | Key behavior |
     * | --- | --- | --- |
     * | Windows | WebView2 `DeleteCookie` | Deletes the cookie whose name and domain/path pair match. |
     * | Linux | WebKitGTK `webkit_cookie_manager_delete_cookie` | Deletes the matching `SoupCookie` from WebKitGTK storage. |
     * | macOS | `WKHTTPCookieStore.deleteCookie` | Deletes the matching `HTTPCookie` asynchronously, bridged here as a blocking call. |
     */
    public fun removeCookie(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        impl.remove(uri, cookie)
    }

    /**
     * Removes all cookies from the desktop WebView cookie store.
     *
     * Other WebViews sharing the same native profile or data store may be affected.
     *
     * Platform notes:
     *
     * | Platform | Native operation | Key behavior |
     * | --- | --- | --- |
     * | Windows | WebView2 `DeleteAllCookies` | Clears cookies under the current WebView2 profile. |
     * | Linux | WebKitGTK `webkit_website_data_manager_clear` | Clears WebKit website data of type cookies. |
     * | macOS | `WKHTTPCookieStore.getAllCookies` + `deleteCookie` | Enumerates the store and deletes each cookie. |
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
     *
     * Platform notes:
     *
     * | Platform | Native operation | Key behavior |
     * | --- | --- | --- |
     * | Windows | WebView2 `GetCookies(uri)` | Returns cookies selected by WebView2 for the requested URI. |
     * | Linux | WebKitGTK `webkit_cookie_manager_get_cookies` | Returns cookies selected by WebKitGTK/libsoup for the requested URI. |
     * | macOS | `WKHTTPCookieStore.getAllCookies` | Reads all cookies, then JNI filters by domain, path, and `secure`. |
     */
    public fun all(uri: String): Set<Cookie> = impl.all(uri)
}

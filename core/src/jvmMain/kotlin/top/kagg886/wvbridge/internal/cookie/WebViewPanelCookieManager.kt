package top.kagg886.wvbridge.internal.cookie

import top.kagg886.wvbridge.cookie.Cookie
import top.kagg886.wvbridge.internal.WebViewBridgePanel

internal class WebViewPanelCookieManager internal constructor(private val panel: WebViewBridgePanel) {
    fun put(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        panel.putCookie(uri, cookie)
    }

    fun remove(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        panel.removeCookie(uri, cookie)
    }

    fun clearAll() {
        panel.clearAll()
    }

    fun all(uri: String): Set<Cookie> = panel.all(uri)
}

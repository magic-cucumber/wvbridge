package top.kagg886.wvbridge.internal.cookie

import top.kagg886.wvbridge.cookie.Cookie
import top.kagg886.wvbridge.internal.WebViewBridgePanel

public abstract class WebViewPanelCookieManager internal constructor(
    private val panel: WebViewBridgePanel
) {
    public fun put(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        panel.put(uri, cookie)
    }

    public fun remove(uri: String, cookie: Cookie) {
        require(cookie is StructedCookie) { "cookie must be a StructedCookie" }
        panel.remove(uri, cookie)
    }

    public fun clearAll() {
        panel.clearAll()
    }

    public fun all(uri: String): List<Cookie> = panel.all(uri)
}

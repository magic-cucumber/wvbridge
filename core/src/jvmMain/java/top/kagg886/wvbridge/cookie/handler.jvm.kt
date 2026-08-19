package top.kagg886.wvbridge.cookie

import top.kagg886.wvbridge.internal.cookie.WebViewPanelCookieManager

public actual typealias NativeCookieManager = WebViewPanelCookieManager

public actual class CookieManager {
    public actual val impl: NativeCookieManager
        get() = TODO("Not yet implemented")

    public actual fun get(uri: String): List<Cookie> {
        TODO("Not yet implemented")
    }
}

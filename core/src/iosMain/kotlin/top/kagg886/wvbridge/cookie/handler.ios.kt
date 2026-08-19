package top.kagg886.wvbridge.cookie

import platform.WebKit.WKHTTPCookieStore

public actual typealias NativeCookieManager = WKHTTPCookieStore

public actual class CookieManager {
    public actual val impl: NativeCookieManager
        get() = TODO("Not yet implemented")

    public actual fun get(uri: String): List<Cookie> {
        TODO("Not yet implemented")
    }
}

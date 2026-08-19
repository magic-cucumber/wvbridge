package top.kagg886.wvbridge.cookie

import android.webkit.CookieManager as AndroidCookieManager

public actual typealias NativeCookieManager = AndroidCookieManager

public actual class CookieManager {
    public actual val impl: NativeCookieManager
        get() = TODO("Not yet implemented")

    public actual fun get(uri: String): List<Cookie> {
        TODO("Not yet implemented")
    }
}

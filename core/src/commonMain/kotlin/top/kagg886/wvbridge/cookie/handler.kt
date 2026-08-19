package top.kagg886.wvbridge.cookie

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/8/19 09:28
 * ================================================
 */

public expect abstract class NativeCookieManager

public expect class CookieManager {
    public val impl: NativeCookieManager

    public fun get(uri: String): List<Cookie>
}

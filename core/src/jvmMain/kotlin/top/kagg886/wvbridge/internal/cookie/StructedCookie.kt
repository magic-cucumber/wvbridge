package top.kagg886.wvbridge.internal.cookie

import top.kagg886.wvbridge.cookie.Cookie

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/8/19 10:39
 * ================================================
 */
internal class StructedCookie(val dict: Map<String, String>) : Cookie {
    override val name: String
        get() = dict["name"]!!
    override val value: String
        get() = dict["value"]!!
}

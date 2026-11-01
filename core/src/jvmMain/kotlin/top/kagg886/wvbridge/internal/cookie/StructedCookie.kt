package top.kagg886.wvbridge.internal.cookie

import top.kagg886.wvbridge.cookie.Cookie

/**
 * A desktop/JVM cookie backed by a string property map.
 *
 * Use this type to create cookies for
 * [top.kagg886.wvbridge.cookie.WebViewPanelCookieManager.putCookie] or to copy and modify cookies
 * returned by the desktop cookie store. Desktop mutation methods require this concrete type;
 * arbitrary implementations of [Cookie] are not accepted.
 *
 * Supported map keys:
 *
 * | Key | Value |
 * | --- | --- |
 * | `name` | Required, non-empty cookie name. |
 * | `value` | Required cookie value; an empty string is allowed. |
 * | `domain` | Cookie domain; defaults to the target URI host when omitted during mutation. |
 * | `path` | Cookie path; defaults to the target URI's default cookie path when omitted during mutation. |
 * | `expiresTimeStamp` | Expiration time in milliseconds since the Unix epoch, encoded as a decimal string. |
 * | `httpOnly` | Boolean string; defaults to `false`. |
 * | `secure` | Boolean string; defaults to `false`. |
 * | `session` | Boolean string; defaults to `true` when `expiresTimeStamp` is absent. |
 * | `sameSite` | `NONE`, `LAX`, or `STRICT`, subject to native backend support. |
 *
 * Boolean strings accept `true`, `1`, `yes`, `false`, `0`, and `no`, ignoring case. For a persistent
 * cookie, provide `expiresTimeStamp` and omit `session` or set it to `false`. A `session` value of
 * `true` causes the native bridge to ignore the expiration time. Native browser rules determine
 * whether the resulting cookie is accepted.
 *
 * Cookies are identified by name, domain, and path. Writing a cookie with the same identity updates
 * it; cookies with different domains or paths can coexist. Removal uses this identity, not the value.
 *
 * ```kotlin
 * val cookie = StructedCookie(
 *     mapOf(
 *         "name" to "theme",
 *         "value" to "dark",
 *         "path" to "/",
 *         "secure" to "true",
 *         "sameSite" to "LAX",
 *     ),
 * )
 * // manager is a desktop WebViewPanelCookieManager.
 * manager.putCookie("https://example.com/", cookie)
 *
 * val updated = StructedCookie(cookie.dict + ("value" to "light"))
 * manager.putCookie("https://example.com/", updated)
 * ```
 *
 * Construction does not validate or copy [dict]. Missing `name` or `value` entries cause their
 * corresponding getters to throw [NullPointerException]; the native bridge also validates required
 * fields when mutating the store. Prefer an immutable map and create a new instance for changes.
 * Changing the map alone does not update the native cookie store.
 *
 * @param dict Cookie attributes using the exact keys listed above. The expiration key is
 * `expiresTimeStamp`, not `expires`; the public `Cookie.expires` extension formats it as an HTTP-date.
 */
public class StructedCookie(internal val dict: Map<String, String>) : Cookie {
    /** Cookie name from the required `name` entry. */
    override val name: String
        get() = dict["name"]!!

    /** Cookie value from the required `value` entry. */
    override val value: String
        get() = dict["value"]!!
}

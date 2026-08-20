package top.kagg886.wvbridge.cookie

/**
 * Cookie domain attribute, when the current platform exposes it.
 *
 * Desktop and iOS cookies carry this value from their native cookie objects. Android cookies read
 * through `android.webkit.CookieManager.getCookie` do not include attributes, so this property is
 * unsupported on Android.
 */
public expect val Cookie.domain: String?

/**
 * Cookie path attribute, when the current platform exposes it.
 */
public expect val Cookie.path: String?

/**
 * Cookie expiration time as an HTTP-date, when available.
 *
 * The returned value follows the cookie `Expires` attribute format, for example
 * `Wed, 20 Aug 2026 12:00:00 GMT`. Session cookies normally return `null` because they do not have
 * a persistent expiration time.
 */
public expect val Cookie.expires: String?

/**
 * Whether this cookie is marked `HttpOnly`, when the current platform exposes it.
 */
public expect val Cookie.httpOnly: Boolean?

/**
 * Whether this cookie is marked `Secure`, when the current platform exposes it.
 */
public expect val Cookie.secure: Boolean?

/**
 * Whether this cookie is a session cookie, when the current platform exposes it.
 */
public expect val Cookie.session: Boolean?

/**
 * Cookie `SameSite` attribute, usually `NONE`, `LAX`, or `STRICT`, when available.
 */
public expect val Cookie.sameSite: String?

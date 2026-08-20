package top.kagg886.wvbridge.cookie

import platform.Foundation.NSDate
import platform.Foundation.NSDateFormatter
import platform.Foundation.NSLocale
import platform.Foundation.NSTimeZone
import platform.Foundation.localeWithLocaleIdentifier
import platform.Foundation.timeZoneWithName

public actual val Cookie.domain: String?
    get() = platformCookie?.impl?.domain

public actual val Cookie.path: String?
    get() = platformCookie?.impl?.path

public actual val Cookie.expires: String?
    get() = platformCookie?.impl?.expiresDate?.toHttpDate()

public actual val Cookie.httpOnly: Boolean?
    get() = platformCookie?.impl?.HTTPOnly

public actual val Cookie.secure: Boolean?
    get() = platformCookie?.impl?.secure

public actual val Cookie.session: Boolean?
    get() = platformCookie?.impl?.sessionOnly

public actual val Cookie.sameSite: String?
    get() = platformCookie?.impl?.sameSitePolicy

private val Cookie.platformCookie: PlatformCookie?
    get() = this as? PlatformCookie

private fun NSDate.toHttpDate(): String {
    return httpDateFormatter.stringFromDate(this)
}

private val httpDateFormatter: NSDateFormatter by lazy {
    NSDateFormatter().apply {
        locale = NSLocale.localeWithLocaleIdentifier("en_US_POSIX")
        timeZone = NSTimeZone.timeZoneWithName("GMT")!!
        dateFormat = "EEE, dd MMM yyyy HH:mm:ss 'GMT'"
    }
}

package top.kagg886.wvbridge.cookie

import top.kagg886.wvbridge.internal.cookie.StructedCookie
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

public actual val Cookie.domain: String?
    get() = structuredValue("domain")

public actual val Cookie.path: String?
    get() = structuredValue("path")

public actual val Cookie.expires: String?
    get() = structuredValue("expiresTimeStamp")?.epochMillisToHttpDateOrNull()

public actual val Cookie.httpOnly: Boolean?
    get() = structuredValue("httpOnly")?.toCookieBooleanOrNull()

public actual val Cookie.secure: Boolean?
    get() = structuredValue("secure")?.toCookieBooleanOrNull()

public actual val Cookie.session: Boolean?
    get() = structuredValue("session")?.toCookieBooleanOrNull()

public actual val Cookie.sameSite: String?
    get() = structuredValue("sameSite")

private fun Cookie.structuredValue(name: String): String? {
    return (this as? StructedCookie)?.dict?.get(name)
}

private fun String.toCookieBooleanOrNull(): Boolean? {
    return when (lowercase()) {
        "true", "1", "yes" -> true
        "false", "0", "no" -> false
        else -> null
    }
}

private fun String.epochMillisToHttpDateOrNull(): String? {
    val epochMillis = toLongOrNull() ?: return null
    return HTTP_DATE_FORMATTER.format(Instant.ofEpochMilli(epochMillis))
}

private val HTTP_DATE_FORMATTER: DateTimeFormatter = DateTimeFormatter
    .ofPattern("EEE, dd MMM yyyy HH:mm:ss 'GMT'", Locale.US)
    .withZone(ZoneId.of("GMT"))

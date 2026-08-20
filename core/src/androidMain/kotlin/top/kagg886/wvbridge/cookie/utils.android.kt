package top.kagg886.wvbridge.cookie

public actual val Cookie.domain: String?
    get() = unsupported("domain")

public actual val Cookie.path: String?
    get() = unsupported("path")

public actual val Cookie.expires: String?
    get() = unsupported("expires")

public actual val Cookie.httpOnly: Boolean?
    get() = unsupported("httpOnly")

public actual val Cookie.secure: Boolean?
    get() = unsupported("secure")

public actual val Cookie.session: Boolean?
    get() = unsupported("session")

public actual val Cookie.sameSite: String?
    get() = unsupported("sameSite")

private fun unsupported(name: String): Nothing {
    throw UnsupportedOperationException("Cookie.$name in android is not supported")
}

package top.kagg886.wvbridge.js.internal

import kotlin.io.encoding.Base64

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/6/25 09:11
 * ================================================
 */

internal fun String.base64Decode(): String = Base64.decode(this).decodeToString()
internal fun String.base64Encode(): String = Base64.encode(this.encodeToByteArray())

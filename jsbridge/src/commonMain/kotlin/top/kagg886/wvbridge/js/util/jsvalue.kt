package top.kagg886.wvbridge.js.util

import top.kagg886.wvbridge.js.protocol.JSValue

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/6/26 07:57
 * ================================================
 */

/**
 * Returns `true` when this value is JavaScript `undefined` or `null`.
 */
public val JSValue.isNull: Boolean
    get() = this is JSValue.Undefined || this is JSValue.Null

/**
 * Returns this value as a JavaScript string primitive.
 *
 * @throws IllegalArgumentException when this value is not a JavaScript string primitive.
 */
public fun JSValue.asString(): String {
    return asStringOrNull() ?: conversionError("string")
}

/**
 * Returns this value as a JavaScript string primitive, or `null` when it is not one.
 */
public fun JSValue.asStringOrNull(): String? {
    return if (this is JSValue.ScriptObject && type == "string") value else null
}

/**
 * Returns this value as a JavaScript boolean primitive.
 *
 * @throws IllegalArgumentException when this value is not a JavaScript boolean primitive.
 */
public fun JSValue.asBoolean(): Boolean {
    return asBooleanOrNull() ?: conversionError("boolean")
}

/**
 * Returns this value as a JavaScript boolean primitive, or `null` when it is not one.
 */
public fun JSValue.asBooleanOrNull(): Boolean? {
    return if (this is JSValue.ScriptObject && type == "boolean") value.toBooleanStrictOrNull() else null
}

/**
 * Returns this value as a JavaScript number primitive.
 *
 * JavaScript `number` maps to Kotlin [Double].
 *
 * @throws IllegalArgumentException when this value is not a JavaScript number primitive.
 */
public fun JSValue.asDouble(): Double {
    return asDoubleOrNull() ?: conversionError("number")
}

/**
 * Returns this value as a JavaScript number primitive, or `null` when it is not one.
 *
 * JavaScript `number` maps to Kotlin [Double].
 */
public fun JSValue.asDoubleOrNull(): Double? {
    return if (this is JSValue.ScriptObject && type == "number") value.toDoubleOrNull() else null
}

private fun JSValue.conversionError(expectedType: String): Nothing {
    throw IllegalArgumentException("Cannot convert $this to JavaScript $expectedType primitive")
}

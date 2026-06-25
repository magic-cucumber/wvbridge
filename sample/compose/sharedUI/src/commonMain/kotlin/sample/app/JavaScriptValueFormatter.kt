package sample.app

import top.kagg886.wvbridge.js.protocol.JSValue

internal fun JSValue.formatForDisplay(): String = when (this) {
    JSValue.Null -> "null"
    JSValue.Undefined -> "undefined"
    is JSValue.Error -> stacktrace
    is JSValue.ScriptObject -> "$type: $value"
    is JSValue.Serializable -> value
}

package sample.app

import top.kagg886.wvbridge.js.protocol.Value

internal fun Value.formatForDisplay(): String = when (this) {
    Value.Null -> "null"
    Value.Undefined -> "undefined"
    is Value.Error -> stacktrace
    is Value.ScriptObject -> "$type: $value"
    is Value.Serializable -> value
}

package top.kagg886.wvbridge.js

/**
 * The JavaScript evaluation result normalized across native WebView backends.
 */
public sealed interface Value {
    /**
     * JavaScript returned `undefined`.
     */
    public data object Undefined : Value

    /**
     * JavaScript returned `null`.
     */
    public data object Null : Value

    /**
     * A JavaScript object that cannot be faithfully represented by JSON.
     *
     * [type] is the result of JavaScript `Object.prototype.toString.call(value)` for objects
     * and functions, or `typeof value` for primitive values.
     * [value] is the result of JavaScript `String(value)`, for example `console` or `window`.
     */
    public data class ScriptObject(val type: String, val value: String) : Value

    /**
     * A JavaScript value serialized with `JSON.stringify`.
     *
     * [value] is the result of JavaScript `JSON.stringify(value)`.
     */
    public data class Serializable(val value: String) : Value

    /**
     * JavaScript evaluation threw an exception.
     *
     * [stacktrace] is the JavaScript stack trace when the runtime exposes one.
     */
    public data class Error(val stacktrace: String) : Value
}

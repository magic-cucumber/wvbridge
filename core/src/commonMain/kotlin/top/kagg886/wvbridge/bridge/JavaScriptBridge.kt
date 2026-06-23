package top.kagg886.wvbridge.bridge

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/6/18 17:31
 * ================================================
 */
public interface JavaScriptBridge {
    public suspend fun evaluateScript(script: String): Value
    public suspend fun registerDocumentStartHook(script: String): CloseHandle

    public sealed interface Value {
        public data object Undefined : Value
        public data object Null : Value
        public data class ScriptObject(val value: String) : Value
    }
}

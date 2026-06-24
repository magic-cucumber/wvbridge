package top.kagg886.wvbridge.js

import top.kagg886.wvbridge.bridge.JavaScriptBridge

/**
 * Evaluates [script] as a JavaScript function body and normalizes the result to [Value].
 *
 * The wrapper sends [script] to the WebView as UTF-8 Base64, decodes it in the page, and executes
 * it with `Function(script).apply(globalThis)`. Because [script] is compiled as a function body,
 * callers must use `return` to produce a value.
 */
public suspend fun JavaScriptBridge.evaluateScriptValue(script: String): Value =
    evaluateScript(buildJavaScriptBridgeEvaluationScript(script)).toJavaScriptBridgeValue()

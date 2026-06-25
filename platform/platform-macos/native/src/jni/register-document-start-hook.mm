#import "javascript-helpers.h"

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    LOGGER_I("registerDocumentStartHook: handle=%lld", (long long) handle);
    if (script == nullptr) {
        LOGGER_W("registerDocumentStartHook: script is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    auto *ctx = require_context(env, handle, "registerDocumentStartHook");
    if (!ctx) return 0;

    NSString *source = jstring_to_nsstring(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_E("registerDocumentStartHook: JNI exception after string conversion");
        return 0;
    }
    LOGGER_V("registerDocumentStartHook: source length=%lu", (unsigned long) [source length]);

    __block jlong hookId = 0;
    __block bool webViewAvailable = true;
    LOGGER_V("registerDocumentStartHook: dispatching to main thread");
    runOnMainSync(^{
        if (!ctx->webView) {
            LOGGER_E("registerDocumentStartHook: webView is not available in main block");
            webViewAvailable = false;
            return;
        }
        if (!ctx->documentStartHooks) {
            ctx->documentStartHooks = [[NSMutableDictionary alloc] init];
            LOGGER_V("registerDocumentStartHook: allocated new documentStartHooks dictionary");
        }

        hookId = ctx->nextDocumentStartHookId++;
        ctx->documentStartHooks[@(hookId)] = source ?: @"";
        add_document_start_script(ctx->webView, source);
        LOGGER_V("registerDocumentStartHook: registered hook id=%lld", (long long) hookId);
    });

    if (!webViewAvailable) {
        LOGGER_E("registerDocumentStartHook: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return 0;
    }
    return hookId;
}

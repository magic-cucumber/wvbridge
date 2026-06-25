#import "javascript-helpers.h"

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    LOGGER_I("unregisterDocumentStartHook: handle=%lld hookId=%lld", (long long) handle, (long long) hookId);
    auto *ctx = require_context(env, handle, "unregisterDocumentStartHook");
    if (!ctx) return;

    __block bool webViewAvailable = true;
    LOGGER_V("unregisterDocumentStartHook: dispatching to main thread");
    runOnMainSync(^{
        if (!ctx->webView) {
            LOGGER_E("unregisterDocumentStartHook: webView is not available in main block");
            webViewAvailable = false;
            return;
        }
        [ctx->documentStartHooks removeObjectForKey:@(hookId)];
        rebuild_document_start_scripts(ctx);
        LOGGER_V("unregisterDocumentStartHook: removed hook id=%lld, rebuilt scripts", (long long) hookId);
    });

    if (!webViewAvailable) {
        LOGGER_E("unregisterDocumentStartHook: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

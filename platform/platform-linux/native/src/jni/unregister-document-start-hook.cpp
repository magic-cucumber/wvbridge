#include "javascript-helpers.h"

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    LOGGER_I("unregisterDocumentStartHook: handle=%lld hookId=%lld", (long long)handle, (long long)hookId);

    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    bool webviewAvailable = true;
    LOGGER_V("unregisterDocumentStartHook: dispatching to GTK thread");
    wvbridge::gtk_run_on_thread_sync([ctx, hookId, &webviewAvailable] {
        if (!ctx->webview) {
            LOGGER_V("unregisterDocumentStartHook: ctx->webview is null in GTK thread");
            webviewAvailable = false;
            return;
        }

        auto it = ctx->document_start_hooks.find(hookId);
        if (it == ctx->document_start_hooks.end()) {
            LOGGER_V("unregisterDocumentStartHook: hookId=%lld not found", (long long)hookId);
            return;
        }

        WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
        webkit_user_content_manager_remove_script(manager, it->second);
        webkit_user_script_unref(it->second);
        ctx->document_start_hooks.erase(it);
        LOGGER_V("unregisterDocumentStartHook: hookId=%lld removed", (long long)hookId);
    });

    if (!webviewAvailable) {
        LOGGER_E("unregisterDocumentStartHook: webview not available");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

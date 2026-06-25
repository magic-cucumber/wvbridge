#include "javascript-helpers.h"

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    LOGGER_I("registerDocumentStartHook: handle=%lld", (long long)handle);

    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (script == nullptr) {
        LOGGER_E("registerDocumentStartHook: null script, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    const std::string source = jstring_to_string(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_W("registerDocumentStartHook: JVM exception after jstring_to_string, aborting");
        return 0;
    }
    LOGGER_V("registerDocumentStartHook: source len=%zu", source.size());

    jlong hookId = 0;
    LOGGER_V("registerDocumentStartHook: dispatching to GTK thread");
    wvbridge::gtk_run_on_thread_sync([ctx, source, &hookId] {
        if (!ctx->webview) {
            LOGGER_V("registerDocumentStartHook: ctx->webview is null in GTK thread");
            return;
        }
        hookId = ctx->next_document_start_hook_id++;
        ctx->document_start_hooks[hookId] = add_document_start_script(ctx->webview, source);
        LOGGER_V("registerDocumentStartHook: hookId=%lld", (long long)hookId);
    });

    if (hookId == 0) {
        LOGGER_E("registerDocumentStartHook: hookId is 0, webview not available");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
    return hookId;
}

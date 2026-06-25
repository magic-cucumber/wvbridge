#include "javascript-helpers.h"

#include <wvbridge/javascript.h>

API_EXPORT(jlong, registerWebMessageHandler, jlong handle, jobject callback) {
    LOGGER_I("registerWebMessageHandler: handle=%lld callback=%p", (long long)handle, callback);
    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (callback == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "callback is null");
        return 0;
    }

    HRESULT hr = S_OK;
    webview2_thread_run_sync(ctx->thread, [ctx, &hr] {
        hr = ensure_web_message_registered(ctx);
    });
    if (FAILED(hr)) {
        throw_hresult(env, "add_WebMessageReceived", hr);
        return 0;
    }

    jlong handlerId = wvbridge::register_web_message_handler(
        env,
        ctx->web_message_handlers_mutex,
        ctx->web_message_handlers,
        ctx->next_web_message_handler_id,
        callback
    );
    if (handlerId == 0) {
        throw_jni_exception(env, "java/lang/RuntimeException", "failed to retain callback");
    }
    return handlerId;
}

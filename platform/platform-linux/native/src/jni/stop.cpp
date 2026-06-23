#include "libs_helpers.h"

API_EXPORT(void, stop, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    bool ok = true;
    std::string error;
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        webkit_web_view_stop_loading(ctx->webview);
    });
    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

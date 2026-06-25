#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, refresh, jlong handle) {
    LOGGER_I("refresh: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("refresh: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    LOGGER_V("refresh: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("refresh: ctx is null after cast, aborting");
        return;
    }

    bool ok = true;
    std::string error;

    LOGGER_V("refresh: running on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("refresh: webview is not available");
            ok = false;
            error = "webview is not available";
            return;
        }

        LOGGER_V("refresh: calling webkit_web_view_reload");
        webkit_web_view_reload(ctx->webview);
    });
    if (!ok) {
        LOGGER_E("refresh: failed, error=%s", error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

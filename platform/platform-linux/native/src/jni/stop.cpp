#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, stop, jlong handle) {
    LOGGER_I("stop: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("stop: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    LOGGER_V("stop: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("stop: ctx is null after cast, aborting");
        return;
    }

    bool ok = true;
    std::string error;

    LOGGER_V("stop: running on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("stop: webview is not available");
            ok = false;
            error = "webview is not available";
            return;
        }

        LOGGER_V("stop: calling webkit_web_view_stop_loading");
        webkit_web_view_stop_loading(ctx->webview);
    });
    if (!ok) {
        LOGGER_E("stop: failed, error=%s", error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, requestFocus0, jlong handle) {
    LOGGER_I("requestFocus0: handle=%lld", (long long)handle);
    (void) thiz;

    if (handle == 0) {
        LOGGER_E("requestFocus0: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    LOGGER_V("requestFocus0: casting handle to WebViewContext");
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) {
        LOGGER_W("requestFocus0: ctx is null after cast, aborting");
        return;
    }

    LOGGER_V("requestFocus0: running focus_embedded_webview on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        focus_embedded_webview(ctx);
    });
}

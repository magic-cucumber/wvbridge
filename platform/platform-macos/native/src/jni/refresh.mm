#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, refresh, jlong handle) {
    LOGGER_I("refresh: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("refresh: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("refresh: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("refresh: ctx is null after cast, aborting");
        return;
    }

    __block bool ok = true;
    LOGGER_V("refresh: dispatching reload to main thread");
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            LOGGER_E("refresh: webView is not available in main block");
            ok = false;
            return;
        }

        [ctx->webView reload];
        LOGGER_V("refresh: reload executed on main thread");
    });
    if (!ok) {
        LOGGER_E("refresh: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

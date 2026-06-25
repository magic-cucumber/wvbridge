#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, stop, jlong handle) {
    LOGGER_I("stop: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("stop: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("stop: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("stop: ctx is null after cast, aborting");
        return;
    }

    __block bool ok = true;
    LOGGER_V("stop: dispatching stopLoading to main thread");
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            LOGGER_E("stop: webView is not available in main block");
            ok = false;
            return;
        }

        [ctx->webView stopLoading];
        LOGGER_V("stop: stopLoading executed on main thread");
    });
    if (!ok) {
        LOGGER_E("stop: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

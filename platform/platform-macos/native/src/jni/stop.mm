#include "libs_helpers.h"

API_EXPORT(void, stop, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    __block bool ok = true;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }

        [ctx->webView stopLoading];
    });
    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

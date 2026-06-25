#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goForward, jlong handle) {
    LOGGER_I("goForward: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("goForward: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("goForward: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("goForward: ctx is null after cast, aborting");
        return JNI_FALSE;
    }

    __block BOOL can_go_forward_after = NO;
    __block bool ok = true;
    __block bool cannot_go_forward = false;
    LOGGER_V("goForward: dispatching goForward to main thread");
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            LOGGER_E("goForward: webView is not available in main block");
            ok = false;
            return;
        }
        if (![ctx->webView canGoForward]) {
            LOGGER_E("goForward: webView cannot go forward");
            ok = false;
            cannot_go_forward = true;
            return;
        }

        [ctx->webView goForward];
        can_go_forward_after = [ctx->webView canGoForward];
        LOGGER_V("goForward: goForward executed, can_go_forward_after=%d", (int) can_go_forward_after);
    });
    if (!ok) {
        LOGGER_E("goForward: throwing exception, cannot_go_forward=%d", (int) cannot_go_forward);
        throw_jni_exception(env, cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            cannot_go_forward ? "webview cannot go forward" : "webview is not available");
        return JNI_FALSE;
    }
    LOGGER_V("goForward: returning %d", (int) (can_go_forward_after ? JNI_TRUE : JNI_FALSE));
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

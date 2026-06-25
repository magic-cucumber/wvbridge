#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goBack, jlong handle) {
    LOGGER_I("goBack: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("goBack: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("goBack: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("goBack: ctx is null after cast, aborting");
        return JNI_FALSE;
    }

    __block BOOL can_go_back_after = NO;
    __block bool ok = true;
    __block bool cannot_go_back = false;
    LOGGER_V("goBack: dispatching goBack to main thread");
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            LOGGER_E("goBack: webView is not available in main block");
            ok = false;
            return;
        }
        if (![ctx->webView canGoBack]) {
            LOGGER_E("goBack: webView cannot go back");
            ok = false;
            cannot_go_back = true;
            return;
        }

        [ctx->webView goBack];
        can_go_back_after = [ctx->webView canGoBack];
        LOGGER_V("goBack: goBack executed, can_go_back_after=%d", (int) can_go_back_after);
    });
    if (!ok) {
        LOGGER_E("goBack: throwing exception, cannot_go_back=%d", (int) cannot_go_back);
        throw_jni_exception(env, cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            cannot_go_back ? "webview cannot go back" : "webview is not available");
        return JNI_FALSE;
    }
    LOGGER_V("goBack: returning %d", (int) (can_go_back_after ? JNI_TRUE : JNI_FALSE));
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

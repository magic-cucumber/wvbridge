#include "libs_helpers.h"

API_EXPORT(jboolean, goBack, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return JNI_FALSE;

    __block BOOL can_go_back_after = NO;
    __block bool ok = true;
    __block bool cannot_go_back = false;
    runOnMainSync(^{
        if (!ctx || !ctx->webView) {
            ok = false;
            return;
        }
        if (![ctx->webView canGoBack]) {
            ok = false;
            cannot_go_back = true;
            return;
        }

        [ctx->webView goBack];
        can_go_back_after = [ctx->webView canGoBack];
    });
    if (!ok) {
        throw_jni_exception(env, cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            cannot_go_back ? "webview cannot go back" : "webview is not available");
        return JNI_FALSE;
    }
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

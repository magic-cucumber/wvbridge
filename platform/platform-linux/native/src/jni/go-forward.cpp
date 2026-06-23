#include "libs_helpers.h"

API_EXPORT(jboolean, goForward, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return JNI_FALSE;

    gboolean can_go_forward_after = FALSE;
    bool ok = true;
    bool cannot_go_forward = false;
    std::string error;
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }
        if (!webkit_web_view_can_go_forward(ctx->webview)) {
            ok = false;
            cannot_go_forward = true;
            error = "webview cannot go forward";
            return;
        }

        webkit_web_view_go_forward(ctx->webview);
        can_go_forward_after = webkit_web_view_can_go_forward(ctx->webview);
    });
    if (!ok) {
        throw_jni_exception(env, cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            error.c_str());
        return JNI_FALSE;
    }
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

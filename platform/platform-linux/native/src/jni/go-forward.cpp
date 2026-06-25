#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goForward, jlong handle) {
    LOGGER_I("goForward: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("goForward: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    LOGGER_V("goForward: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("goForward: ctx is null after cast, aborting");
        return JNI_FALSE;
    }

    gboolean can_go_forward_after = FALSE;
    bool ok = true;
    bool cannot_go_forward = false;
    std::string error;

    LOGGER_V("goForward: running on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("goForward: webview is not available");
            ok = false;
            error = "webview is not available";
            return;
        }
        if (!webkit_web_view_can_go_forward(ctx->webview)) {
            LOGGER_V("goForward: webview cannot go forward");
            ok = false;
            cannot_go_forward = true;
            error = "webview cannot go forward";
            return;
        }

        LOGGER_V("goForward: calling webkit_web_view_go_forward");
        webkit_web_view_go_forward(ctx->webview);
        can_go_forward_after = webkit_web_view_can_go_forward(ctx->webview);
    });
    if (!ok) {
        LOGGER_E("goForward: failed, error=%s", error.c_str());
        throw_jni_exception(env, cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            error.c_str());
        return JNI_FALSE;
    }
    LOGGER_V("goForward: can_go_forward_after=%d", can_go_forward_after);
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

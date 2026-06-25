#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goBack, jlong handle) {
    LOGGER_I("goBack: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("goBack: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }
    LOGGER_V("goBack: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("goBack: ctx is null after cast, aborting");
        return JNI_FALSE;
    }

    gboolean can_go_back_after = FALSE;
    bool ok = true;
    bool cannot_go_back = false;
    std::string error;

    LOGGER_V("goBack: running on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("goBack: webview is not available");
            ok = false;
            error = "webview is not available";
            return;
        }
        if (!webkit_web_view_can_go_back(ctx->webview)) {
            LOGGER_V("goBack: webview cannot go back");
            ok = false;
            cannot_go_back = true;
            error = "webview cannot go back";
            return;
        }

        LOGGER_V("goBack: calling webkit_web_view_go_back");
        webkit_web_view_go_back(ctx->webview);
        can_go_back_after = webkit_web_view_can_go_back(ctx->webview);
    });
    if (!ok) {
        LOGGER_E("goBack: failed, error=%s", error.c_str());
        throw_jni_exception(env, cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException",
                            error.c_str());
        return JNI_FALSE;
    }
    LOGGER_V("goBack: can_go_back_after=%d", can_go_back_after);
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

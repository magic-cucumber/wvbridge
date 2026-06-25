#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, requestFocus0, jlong handle) {
    LOGGER_I("requestFocus0: handle=%lld", (long long)handle);
    (void) thiz;

    if (handle == 0) {
        LOGGER_E("requestFocus0: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("requestFocus0: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("requestFocus0: context or thread is null, aborting");
        return;
    }

    LOGGER_V("requestFocus0: dispatching to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_V("requestFocus0: ctx closing or null, aborting");
            return;
        }
        if (ctx->child_hwnd && IsWindow(ctx->child_hwnd)) {
            LOGGER_V("requestFocus0: calling SetFocus on child_hwnd=%p", ctx->child_hwnd);
            SetFocus(ctx->child_hwnd);
        }
        if (ctx->controller) {
            LOGGER_V("requestFocus0: calling MoveFocus PROGRAMMATIC");
            ctx->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    });
}

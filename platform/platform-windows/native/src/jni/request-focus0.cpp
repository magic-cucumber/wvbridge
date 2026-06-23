#include "libs_helpers.h"

API_EXPORT(void, requestFocus0, jlong handle) {
    (void) thiz;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    webview2_thread_run_sync(ctx->thread, [ctx] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (ctx->child_hwnd && IsWindow(ctx->child_hwnd)) {
            SetFocus(ctx->child_hwnd);
        }
        if (ctx->controller) {
            ctx->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    });
}

#include "libs_helpers.h"

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    (void) thiz;
    (void) x;
    (void) y;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    const int width = clamp_dim(w);
    const int height = clamp_dim(h);

    webview2_thread_run_sync(ctx->thread, [ctx, width, height] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (!ctx->child_hwnd || !IsWindow(ctx->child_hwnd)) return;

        SetWindowPos(
            ctx->child_hwnd,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );

        if (ctx->controller) {
            RECT bounds{0, 0, width, height};
            ctx->controller->put_Bounds(bounds);
            ctx->controller->put_IsVisible(TRUE);
        }
    });
}

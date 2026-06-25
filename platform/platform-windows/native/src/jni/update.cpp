#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    LOGGER_I("update: handle=%lld w=%d h=%d x=%d y=%d", (long long)handle, w, h, x, y);
    (void) thiz;
    (void) x;
    (void) y;

    if (handle == 0) {
        LOGGER_E("update: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("update: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("update: context or thread is null, aborting");
        return;
    }

    const int width = clamp_dim(w);
    const int height = clamp_dim(h);
    LOGGER_V("update: clamped dimensions width=%d height=%d", width, height);

    webview2_thread_run_sync(ctx->thread, [ctx, width, height] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_V("update: ctx closing or null, aborting SetWindowPos");
            return;
        }
        if (!ctx->child_hwnd || !IsWindow(ctx->child_hwnd)) {
            LOGGER_V("update: child_hwnd is invalid, aborting");
            return;
        }

        LOGGER_V("update: calling SetWindowPos width=%d height=%d", width, height);
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
            LOGGER_V("update: setting controller bounds (%d,%d,%d,%d)", bounds.left, bounds.top, bounds.right, bounds.bottom);
            ctx->controller->put_Bounds(bounds);
            ctx->controller->put_IsVisible(TRUE);
        }
    });
}

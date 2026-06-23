#include "libs_helpers.h"

API_EXPORT(void, close0, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    ctx->closing.store(true, std::memory_order_release);

    // 4) 销毁必要信息
    wvbridge::gtk_run_on_thread_sync([&] {
        // parent 可能已先于 WebView 被 AWT 销毁，此时 GTK/GDK 在 destroy/unmap 时容易触发 BadWindow。
        x11_ignore_errors([&] {
            // 尝试先从 AWT parent 脱离（best-effort）。
            if (ctx->xdisplay && ctx->child_xid != 0) {
                ::Window root = DefaultRootWindow(ctx->xdisplay);
                if (root != 0) {
                    XReparentWindow(ctx->xdisplay, ctx->child_xid, root, 0, 0);
                    XUnmapWindow(ctx->xdisplay, ctx->child_xid);
                    XFlush(ctx->xdisplay);
                }
            }

            destroy_ctx_on_gtk_thread(ctx);
        });

        // 让销毁过程产生的 idle/finalize 在退出主循环前跑一轮，降低 WebKit/EGL 清理时序问题。
        while (g_main_context_pending(nullptr)) {
            g_main_context_iteration(nullptr, FALSE);
        }
    });

    delete ctx;
}

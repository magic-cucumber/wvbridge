#include "libs_helpers.h"

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    (void) thiz;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    // x/y 在 reparent 语义下应恒为 (0,0)。
    (void) x;
    (void) y;

    const int cw = clamp_dim(w);
    const int ch = clamp_dim(h);

    // 必须同步：否则 close0 可能先 delete ctx，导致异步闭包访问悬空指针。
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

        int tw = cw;
        int th = ch;

        // 以 parent_xid 的真实几何为准（可修复 HiDPI/逻辑像素与物理像素不一致导致的黑边）。
        if (ctx->xdisplay && ctx->parent_xid != 0) {
            unsigned int pw = 0, ph = 0;
            ::Window root = 0;
            int rx = 0, ry = 0;
            unsigned int bw = 0, depth = 0;
            if (XGetGeometry(ctx->xdisplay, ctx->parent_xid, &root, &rx, &ry, &pw, &ph, &bw, &depth) != 0) {
                tw = clamp_dim((jint) pw);
                th = clamp_dim((jint) ph);
            }
        }

        if (ctx->webview) {
            int scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(ctx->window));
            gtk_widget_set_size_request(GTK_WIDGET(ctx->webview), tw / scale_factor, th / scale_factor);
            gtk_widget_queue_resize(GTK_WIDGET(ctx->webview));
        }

        if (ctx->xdisplay && ctx->child_xid != 0) {
            x11_ignore_errors([&] {
                XMoveResizeWindow(ctx->xdisplay, ctx->child_xid, 0, 0,
                                  (unsigned int) tw, (unsigned int) th);
                XFlush(ctx->xdisplay);
            });
        }
    });
}

#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, update, jlong handle, jint w, jint h, jint x, jint y) {
    LOGGER_I("update: handle=%lld w=%d h=%d x=%d y=%d", (long long)handle, w, h, x, y);
    (void) thiz;

    if (handle == 0) {
        LOGGER_E("update: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    LOGGER_V("update: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("update: ctx is null after cast, aborting");
        return;
    }

    (void) x;
    (void) y;

    const int cw = clamp_dim(w);
    const int ch = clamp_dim(h);
    LOGGER_V("update: clamped dimensions cw=%d ch=%d", cw, ch);

    LOGGER_V("update: running on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_V("update: ctx is null or closing, aborting GTK work");
            return;
        }

        int tw = cw;
        int th = ch;

        if (ctx->foreign_parent_window && !gdk_window_is_destroyed(ctx->foreign_parent_window)) {
            LOGGER_V("update: querying tracked foreign parent geometry window=%p xid=%lu",
                     ctx->foreign_parent_window, (unsigned long)ctx->parent_xid);
            int pw = 0;
            int ph = 0;
            gdk_window_get_geometry(ctx->foreign_parent_window, nullptr, nullptr, &pw, &ph);
            if (pw > 0 && ph > 0) {
                tw = clamp_dim((jint) pw);
                th = clamp_dim((jint) ph);
                LOGGER_V("update: parent geometry pw=%d ph=%d -> tw=%d th=%d", pw, ph, tw, th);
            } else {
                LOGGER_W("update: tracked parent returned invalid geometry pw=%d ph=%d; using requested size", pw, ph);
            }
        } else if (ctx->foreign_parent_window) {
            LOGGER_W("update: tracked AWT parent is destroyed; skipping native resize ctx=%p", ctx);
            return;
        }

        if (ctx->webview) {
            int scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(ctx->window));
            LOGGER_V("update: scale_factor=%d, setting webview size_request to %dx%d", scale_factor, tw / scale_factor, th / scale_factor);
            gtk_widget_set_size_request(GTK_WIDGET(ctx->webview), tw / scale_factor, th / scale_factor);
            gtk_widget_queue_resize(GTK_WIDGET(ctx->webview));
        }

        if (ctx->window) {
            GdkWindow* child = gtk_widget_get_window(ctx->window);
            if (child && !gdk_window_is_destroyed(child)) {
                LOGGER_V("update: moving/resizing tracked child=%p xid=%lu to %dx%d",
                         child, (unsigned long)ctx->child_xid, tw, th);
                gdk_window_move_resize(child, 0, 0, tw, th);
            } else {
                LOGGER_W("update: child GdkWindow unavailable or destroyed child=%p ctx=%p", child, ctx);
            }
        }
    });
}

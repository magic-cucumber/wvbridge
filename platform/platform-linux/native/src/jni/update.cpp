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

        if (ctx->xdisplay && ctx->parent_xid != 0) {
            LOGGER_V("update: querying parent_xid geometry");
            unsigned int pw = 0, ph = 0;
            ::Window root = 0;
            int rx = 0, ry = 0;
            unsigned int bw = 0, depth = 0;
            if (XGetGeometry(ctx->xdisplay, ctx->parent_xid, &root, &rx, &ry, &pw, &ph, &bw, &depth) != 0) {
                tw = clamp_dim((jint) pw);
                th = clamp_dim((jint) ph);
                LOGGER_V("update: parent geometry pw=%u ph=%u -> tw=%d th=%d", pw, ph, tw, th);
            }
        }

        if (ctx->webview) {
            int scale_factor = gtk_widget_get_scale_factor(GTK_WIDGET(ctx->window));
            LOGGER_V("update: scale_factor=%d, setting webview size_request to %dx%d", scale_factor, tw / scale_factor, th / scale_factor);
            gtk_widget_set_size_request(GTK_WIDGET(ctx->webview), tw / scale_factor, th / scale_factor);
            gtk_widget_queue_resize(GTK_WIDGET(ctx->webview));
        }

        if (ctx->xdisplay && ctx->child_xid != 0) {
            LOGGER_V("update: moving/resizing child_xid to %ux%u", (unsigned int)tw, (unsigned int)th);
            x11_ignore_errors([&] {
                XMoveResizeWindow(ctx->xdisplay, ctx->child_xid, 0, 0,
                                  (unsigned int) tw, (unsigned int) th);
                XFlush(ctx->xdisplay);
            });
        }
    });
}

#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, close0, jlong handle) {
    LOGGER_I("close0: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("close0: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    LOGGER_V("close0: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("close0: ctx is null after cast, aborting");
        return;
    }

    LOGGER_V("close0: setting closing flag");
    ctx->closing.store(true, std::memory_order_release);

    LOGGER_V("close0: running destroy on GTK thread");
    wvbridge::gtk_run_on_thread_sync([&] {
        x11_ignore_errors([&] {
            if (ctx->xdisplay && ctx->child_xid != 0) {
                LOGGER_V("close0: reparenting child_xid=%lu to root", (unsigned long)ctx->child_xid);
                ::Window root = DefaultRootWindow(ctx->xdisplay);
                if (root != 0) {
                    XReparentWindow(ctx->xdisplay, ctx->child_xid, root, 0, 0);
                    XUnmapWindow(ctx->xdisplay, ctx->child_xid);
                    XFlush(ctx->xdisplay);
                }
            }

            LOGGER_V("close0: calling destroy_ctx_on_gtk_thread");
            destroy_ctx_on_gtk_thread(ctx);
        });

        LOGGER_V("close0: draining pending GTK events");
        while (g_main_context_pending(nullptr)) {
            g_main_context_iteration(nullptr, FALSE);
        }
    });

    LOGGER_V("close0: deleting ctx");
    delete ctx;
}

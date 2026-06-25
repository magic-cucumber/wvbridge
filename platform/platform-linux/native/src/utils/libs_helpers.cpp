#include "libs_helpers.h"
#include <wvbridge/java_runtime.h>
#include <wvbridge/javascript.h>
#include <wvbridge/logger.h>

int clamp_dim(jint v) {
        LOGGER_V("clamp_dim: v=%d", (int)v);
        return (v < 1) ? 1 : (int) v;
    }

    ::Display *get_xdisplay_from_gdk_default() {
        LOGGER_I("get_xdisplay_from_gdk_default:");
        GdkDisplay *gdpy = gdk_display_get_default();
        if (!gdpy) {
            LOGGER_W("get_xdisplay_from_gdk_default: null GdkDisplay, returning nullptr");
            return nullptr;
        }

        // WebKitGTK 4.1 + JAWT_X11DrawingSurfaceInfo -> 仅支持 X11 嵌入
        if (!GDK_IS_X11_DISPLAY(gdpy)) {
            LOGGER_W("get_xdisplay_from_gdk_default: not X11 display, returning nullptr");
            return nullptr;
        }

        LOGGER_V("get_xdisplay_from_gdk_default: getting X11 xdisplay");
        return gdk_x11_display_get_xdisplay(gdpy);
    }

    ::Window get_xid_from_gtk_window(GtkWidget *w) {
        LOGGER_I("get_xid_from_gtk_window: w=%p", w);
        if (!w) {
            LOGGER_W("get_xid_from_gtk_window: null widget, returning 0");
            return 0;
        }

        GdkWindow *gdk_window = gtk_widget_get_window(w);
        if (!gdk_window || !GDK_IS_X11_WINDOW(gdk_window)) {
            LOGGER_W("get_xid_from_gtk_window: null or non-X11 GdkWindow, returning 0");
            return 0;
        }

        LOGGER_V("get_xid_from_gtk_window: getting X11 xid");
        return (::Window) gdk_x11_window_get_xid(gdk_window);
    }

    // 在 X11 下吞掉由窗口已被外部销毁/重父子关系导致的 BadWindow 等错误，避免 GDK 打 warning。
    void x11_ignore_errors(const std::function<void()> &fn) {
        LOGGER_I("x11_ignore_errors:");
        GdkDisplay *gdpy = gdk_display_get_default();
        if (gdpy && GDK_IS_X11_DISPLAY(gdpy)) {
            LOGGER_V("x11_ignore_errors: X11 display, trapping errors");
            gdk_x11_display_error_trap_push(gdpy);
            if (fn) fn();
            gdk_x11_display_error_trap_pop_ignored(gdpy);
            return;
        }

        LOGGER_V("x11_ignore_errors: non-X11 or null display, running fn directly");
        if (fn) fn();
    }

    // 仅针对 X11：设置 EWMH 属性，避免这个用于嵌入的 GtkWindow 被 GNOME/WM 当作独立应用窗口展示。
    void x11_set_ewmh_embed_hints(::Display *dpy, ::Window win) {
        LOGGER_I("x11_set_ewmh_embed_hints: dpy=%p, win=%lu", dpy, (unsigned long)win);
        if (!dpy || win == 0) {
            LOGGER_W("x11_set_ewmh_embed_hints: null dpy or zero win, aborting");
            return;
        }

        const Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
        const Atom skip_taskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        const Atom skip_pager = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);

        LOGGER_V("x11_set_ewmh_embed_hints: setting _NET_WM_STATE skip_taskbar/skip_pager");
        Atom states[2] = {skip_taskbar, skip_pager};
        XChangeProperty(dpy, win, net_wm_state, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(states), 2);

        // 补充 window type：utility 一般不会出现在任务栏/概览中（不同 WM 行为略有差异）。
        LOGGER_V("x11_set_ewmh_embed_hints: setting _NET_WM_WINDOW_TYPE to UTILITY");
        const Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        const Atom utility = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);

        XChangeProperty(dpy, win, net_wm_window_type, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char *>(const_cast<Atom *>(&utility)), 1);

        XFlush(dpy);
    }

    void destroy_ctx_on_gtk_thread(WebViewContext *ctx) {
        LOGGER_I("destroy_ctx_on_gtk_thread: ctx=%p", ctx);
        if (!ctx) {
            LOGGER_W("destroy_ctx_on_gtk_thread: null ctx, aborting");
            return;
        }

        if (ctx->window && ctx->window_button_press_handler_id != 0) {
            LOGGER_V("destroy_ctx_on_gtk_thread: disconnecting window button press handler");
            g_signal_handler_disconnect(ctx->window, ctx->window_button_press_handler_id);
            ctx->window_button_press_handler_id = 0;
        }
        if (ctx->webview && ctx->webview_button_press_handler_id != 0) {
            LOGGER_V("destroy_ctx_on_gtk_thread: disconnecting webview button press handler");
            g_signal_handler_disconnect(ctx->webview, ctx->webview_button_press_handler_id);
            ctx->webview_button_press_handler_id = 0;
        }
        if (ctx->webview) {
            WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
            if (ctx->web_message_handler_id != 0) {
                LOGGER_V("destroy_ctx_on_gtk_thread: disconnecting web message handler");
                g_signal_handler_disconnect(manager, ctx->web_message_handler_id);
                ctx->web_message_handler_id = 0;
            }
            webkit_user_content_manager_unregister_script_message_handler(manager, "wvbridge");
            LOGGER_V("destroy_ctx_on_gtk_thread: script message handler unregistered");
        }

        LOGGER_V("destroy_ctx_on_gtk_thread: destroying webview events");
        wvbridge::webview_events_destroy(ctx->events);
        ctx->events = nullptr;

        if (ctx->webview) {
            LOGGER_V("destroy_ctx_on_gtk_thread: removing document_start_hooks");
            WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
            for (const auto &entry: ctx->document_start_hooks) {
                webkit_user_content_manager_remove_script(manager, entry.second);
                webkit_user_script_unref(entry.second);
            }
            ctx->document_start_hooks.clear();
        }

        LOGGER_V("destroy_ctx_on_gtk_thread: deleting web message handler refs");
        int attached = 0;
        JNIEnv *env = java_runtime_get_env(&attached);
        if (env) {
            wvbridge::delete_web_message_handler_refs(
                env,
                ctx->web_message_handlers_mutex,
                ctx->web_message_handlers
            );
        }
        java_runtime_detach_env(attached);
        LOGGER_V("destroy_ctx_on_gtk_thread: web message handler refs cleanup done");

        // GtkWindow 销毁后，会连带销毁子树（WebView）。
        if (ctx->window) {
            LOGGER_V("destroy_ctx_on_gtk_thread: destroying gtk window");
            gtk_widget_destroy(ctx->window);
            ctx->window = nullptr;
            ctx->webview = nullptr;
        }

        ctx->child_xid = 0;
        ctx->xdisplay = nullptr;
        ctx->parent_xid = 0;
        LOGGER_V("destroy_ctx_on_gtk_thread: done");
    }

    void focus_embedded_webview(WebViewContext *ctx) {
        LOGGER_I("focus_embedded_webview: ctx=%p", ctx);
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_W("focus_embedded_webview: null ctx or closing, aborting");
            return;
        }

        if (ctx->window && GTK_IS_WINDOW(ctx->window)) {
            LOGGER_V("focus_embedded_webview: setting window focus");
            gtk_window_set_accept_focus(GTK_WINDOW(ctx->window), TRUE);
            gtk_window_set_focus(GTK_WINDOW(ctx->window),
                                 ctx->webview ? GTK_WIDGET(ctx->webview) : GTK_WIDGET(ctx->window));
        }

        if (ctx->webview) {
            LOGGER_V("focus_embedded_webview: grabbing webview focus");
            gtk_widget_grab_focus(GTK_WIDGET(ctx->webview));
        } else if (ctx->window) {
            LOGGER_V("focus_embedded_webview: grabbing window focus");
            gtk_widget_grab_focus(GTK_WIDGET(ctx->window));
        }

        if (ctx->window) {
            GdkWindow *gdk_window = gtk_widget_get_window(ctx->window);
            if (gdk_window) {
                LOGGER_V("focus_embedded_webview: focusing gdk window");
                gdk_window_focus(gdk_window, GDK_CURRENT_TIME);
            }
        }

        if (ctx->xdisplay && ctx->child_xid != 0) {
            LOGGER_V("focus_embedded_webview: setting X input focus");
            x11_ignore_errors([&] {
                XSetInputFocus(ctx->xdisplay, ctx->child_xid, RevertToParent, CurrentTime);
                XFlush(ctx->xdisplay);
            });
        }
    }

    gboolean focus_on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
        (void) widget;
        (void) event;

        auto *ctx = static_cast<WebViewContext *>(user_data);
        LOGGER_I("focus_on_button_press_cb: widget=%p, event=%p, ctx=%p", widget, event, ctx);
        focus_embedded_webview(ctx);
        return FALSE;
    }

#include "libs_helpers.h"
#include <wvbridge/javascript.h>
#include <wvbridge/logger.h>

namespace {

void wvbridge_script_message_received(WebKitUserContentManager *, WebKitJavascriptResult *result, gpointer userData) {
    LOGGER_V("wvbridge_script_message_received: entry");

    auto *ctx = static_cast<WebViewContext *>(userData);
    if (!ctx || !result) return;

    JSCValue *value = webkit_javascript_result_get_js_value(result);
    if (!value || jsc_value_is_undefined(value) || jsc_value_is_null(value)) {
        LOGGER_V("wvbridge_script_message_received: value is undefined/null, dispatching empty message");
        wvbridge::dispatch_web_message_to_java(ctx->web_message_handlers_mutex, ctx->web_message_handlers, "");
        return;
    }

    gchar *stringValue = jsc_value_to_string(value);
    std::string message = stringValue ? stringValue : "";
    LOGGER_V("wvbridge_script_message_received: message=%.100s", message.c_str());
    if (stringValue) g_free(stringValue);
    wvbridge::dispatch_web_message_to_java(
        ctx->web_message_handlers_mutex,
        ctx->web_message_handlers,
        message.c_str()
    );
    LOGGER_V("wvbridge_script_message_received: dispatched");
}

}

API_EXPORT(jlong, initAndAttach) {
    LOGGER_I("initAndAttach: entry");

    if (!wvbridge::gtk_is_inited()) {
        LOGGER_V("initAndAttach: GTK not initialized, calling gtk_init");
        wvbridge::gtk_init();
    }

    ::Window parent_xid = 0;

    LOGGER_V("initAndAttach: acquiring JAWT");
    JAWT awt;
    awt.version = JAWT_VERSION_1_4;
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        LOGGER_E("initAndAttach: JAWT_GetAWT failed");
        throw_jni_exception(env, "java/lang/RuntimeException", "JAWT_GetAWT failed");
        return 0;
    }

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) {
        LOGGER_E("initAndAttach: GetDrawingSurface failed");
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurface failed");
        return 0;
    }
    LOGGER_V("initAndAttach: DrawingSurface acquired ds=%p", (void*)ds);

    bool ds_locked = false;
    JAWT_DrawingSurfaceInfo *dsi = nullptr;

    auto free_dsi = [&] {
        if (dsi) {
            ds->FreeDrawingSurfaceInfo(dsi);
            dsi = nullptr;
        }
    };

    auto unlock_ds = [&] {
        if (ds && ds_locked) {
            ds->Unlock(ds);
            ds_locked = false;
        }
    };

    auto free_ds = [&] {
        if (ds) {
            awt.FreeDrawingSurface(ds);
            ds = nullptr;
        }
    };

    LOGGER_V("initAndAttach: locking DrawingSurface");
    const jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        LOGGER_E("initAndAttach: DrawingSurface lock failed, lock=%d", lock);
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
        return 0;
    }
    ds_locked = true;

    LOGGER_V("initAndAttach: getting DrawingSurfaceInfo");
    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        LOGGER_E("initAndAttach: GetDrawingSurfaceInfo returned null");
        unlock_ds();
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
        return 0;
    }

    LOGGER_V("initAndAttach: reading X11 platform info");
    auto *xinfo = static_cast<JAWT_X11DrawingSurfaceInfo *>(dsi->platformInfo);
    if (xinfo) {
        parent_xid = (::Window) xinfo->drawable;
        LOGGER_V("initAndAttach: parent_xid=%lu", (unsigned long)parent_xid);
    }

    free_dsi();
    unlock_ds();
    free_ds();
    LOGGER_V("initAndAttach: DrawingSurface released");

    if (parent_xid == 0) {
        LOGGER_E("initAndAttach: parent_xid is 0, AWT drawable is null");
        throw_jni_exception(env, "java/lang/RuntimeException", "AWT drawable is null (not an X11 Window?)");
        return 0;
    }

    LOGGER_V("initAndAttach: creating WebViewContext");
    auto *ctx = new WebViewContext();
    ctx->parent_xid = parent_xid;
    const jlong pointer = reinterpret_cast<jlong>(ctx);

    bool ok = true;

    LOGGER_V("initAndAttach: running GTK thread creation");
    wvbridge::gtk_run_on_thread_sync([&] {
        LOGGER_V("initAndAttach: GTK thread - acquiring XDisplay");
        ctx->xdisplay = get_xdisplay_from_gdk_default();
        if (!ctx->xdisplay) {
            LOGGER_W("initAndAttach: GTK thread - get_xdisplay_from_gdk_default returned null");
            ok = false;
            return;
        }

        LOGGER_V("initAndAttach: GTK thread - querying parent geometry");
        unsigned int pw = 1, ph = 1;
        {
            ::Window root = 0;
            int rx = 0, ry = 0;
            unsigned int bw = 0, depth = 0;
            if (XGetGeometry(ctx->xdisplay, ctx->parent_xid, &root, &rx, &ry, &pw, &ph, &bw, &depth) == 0) {
                LOGGER_V("initAndAttach: GTK thread - XGetGeometry failed, using default 1x1");
                pw = 1;
                ph = 1;
            }
        }
        LOGGER_V("initAndAttach: GTK thread - parent geometry pw=%u ph=%u", pw, ph);

        LOGGER_V("initAndAttach: GTK thread - creating GTK window");
        ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(ctx->window), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_accept_focus(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(ctx->window), (int) pw, (int) ph);
        gtk_widget_set_can_focus(GTK_WIDGET(ctx->window), TRUE);
        gtk_widget_add_events(GTK_WIDGET(ctx->window), GDK_BUTTON_PRESS_MASK);

        LOGGER_V("initAndAttach: GTK thread - creating WebKit WebView");
        ctx->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        gtk_widget_set_can_focus(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_hexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_halign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        gtk_widget_set_valign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        gtk_widget_add_events(GTK_WIDGET(ctx->webview), GDK_BUTTON_PRESS_MASK);

        WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
        webkit_user_content_manager_register_script_message_handler(manager, "wvbridge");
        ctx->web_message_handler_id = g_signal_connect(
            manager,
            "script-message-received::wvbridge",
            G_CALLBACK(wvbridge_script_message_received),
            ctx
        );
        LOGGER_V("initAndAttach: GTK thread - wvbridge script message handler registered");

        LOGGER_V("initAndAttach: GTK thread - connecting signals");
        ctx->window_button_press_handler_id = g_signal_connect(
            ctx->window,
            "button-press-event",
            G_CALLBACK(focus_on_button_press_cb),
            ctx
        );
        ctx->webview_button_press_handler_id = g_signal_connect(
            ctx->webview,
            "button-press-event",
            G_CALLBACK(focus_on_button_press_cb),
            ctx
        );

        ctx->events = wvbridge::webview_events_create(ctx->webview, pointer, &ctx->closing);
        LOGGER_V("initAndAttach: GTK thread - webview events created");

        gtk_container_add(GTK_CONTAINER(ctx->window), GTK_WIDGET(ctx->webview));
        LOGGER_V("initAndAttach: GTK thread - realizing window");
        gtk_widget_realize(ctx->window);

        ctx->child_xid = get_xid_from_gtk_window(ctx->window);
        if (ctx->child_xid == 0) {
            LOGGER_W("initAndAttach: GTK thread - get_xid_from_gtk_window returned 0");
            ok = false;
            return;
        }
        LOGGER_V("initAndAttach: GTK thread - child_xid=%lu", (unsigned long)ctx->child_xid);

        LOGGER_V("initAndAttach: GTK thread - reparenting to AWT drawable");
        x11_ignore_errors([&] {
            x11_set_ewmh_embed_hints(ctx->xdisplay, ctx->child_xid);

            XReparentWindow(ctx->xdisplay, ctx->child_xid, ctx->parent_xid, 0, 0);
            XMoveResizeWindow(ctx->xdisplay, ctx->child_xid, 0, 0, pw, ph);
            gtk_widget_show_all(ctx->window);
            XMapRaised(ctx->xdisplay, ctx->child_xid);
            XFlush(ctx->xdisplay);
        });
        LOGGER_V("initAndAttach: GTK thread - reparent complete");
    });

    if (!ok) {
        LOGGER_E("initAndAttach: GTK creation failed, destroying ctx");
        wvbridge::gtk_run_on_thread_sync([&] { destroy_ctx_on_gtk_thread(ctx); });
        delete ctx;
        throw_jni_exception(env, "java/lang/RuntimeException", "Init WebView/attach to AWT failed (X11 only)");
        return 0;
    }

    const jlong handle = reinterpret_cast<jlong>(ctx);
    LOGGER_I("initAndAttach: success, returning handle=%lld", (long long)handle);
    return handle;
}

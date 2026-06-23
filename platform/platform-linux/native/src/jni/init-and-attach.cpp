#include "libs_helpers.h"

API_EXPORT(jlong, initAndAttach) {
    // 确保 GTK 主线程/主循环已初始化（只初始化一次）。
    if (!wvbridge::gtk_is_inited()) {
        wvbridge::gtk_init();
    }

    ::Window parent_xid = 0;

    // JAWT surface layers
    JAWT awt;
    awt.version = JAWT_VERSION_1_4;
    if (JAWT_GetAWT(env, &awt) == JNI_FALSE) {
        throw_jni_exception(env, "java/lang/RuntimeException", "JAWT_GetAWT failed");
        return 0;
    }

    JAWT_DrawingSurface *ds = awt.GetDrawingSurface(env, thiz);
    if (!ds) {
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurface failed");
        return 0;
    }

    // 下面的清理逻辑必须与实际获取顺序严格对称：
    // Lock -> GetDrawingSurfaceInfo -> FreeDrawingSurfaceInfo -> Unlock -> FreeDrawingSurface
    bool ds_locked = false;
    JAWT_DrawingSurfaceInfo *dsi = nullptr;

    auto free_dsi = [&] {
        if (dsi) {
            // JAWT 1.4: FreeDrawingSurfaceInfo 只接收 dsi 参数
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

    const jint lock = ds->Lock(ds);
    if (lock & JAWT_LOCK_ERROR) {
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "DrawingSurface lock failed");
        return 0;
    }
    ds_locked = true;

    dsi = ds->GetDrawingSurfaceInfo(ds);
    if (!dsi) {
        unlock_ds();
        free_ds();
        throw_jni_exception(env, "java/lang/RuntimeException", "GetDrawingSurfaceInfo failed");
        return 0;
    }

    // 读取 X11 宿主窗口句柄
    auto *xinfo = static_cast<JAWT_X11DrawingSurfaceInfo *>(dsi->platformInfo);
    if (xinfo) {
        parent_xid = (::Window) xinfo->drawable;
    }

    free_dsi();
    unlock_ds();
    free_ds();

    if (parent_xid == 0) {
        throw_jni_exception(env, "java/lang/RuntimeException", "AWT drawable is null (not an X11 Window?)");
        return 0;
    }

    // 2) 在 GTK 线程同步创建 WebView，并 reparent 到 AWT 宿主窗口
    auto *ctx = new WebViewContext();
    ctx->parent_xid = parent_xid;
    const jlong pointer = reinterpret_cast<jlong>(ctx);

    bool ok = true;

    wvbridge::gtk_run_on_thread_sync([&] {
        // 仅支持 X11：需要拿到 XDisplay + XID。
        ctx->xdisplay = get_xdisplay_from_gdk_default();
        if (!ctx->xdisplay) {
            ok = false;
            return;
        }

        // 初始大小：取 AWT 宿主窗口当前几何信息（后续 update 也会以 parent 的真实几何为准）。
        unsigned int pw = 1, ph = 1;
        {
            ::Window root = 0;
            int rx = 0, ry = 0;
            unsigned int bw = 0, depth = 0;
            if (XGetGeometry(ctx->xdisplay, ctx->parent_xid, &root, &rx, &ry, &pw, &ph, &bw, &depth) == 0) {
                pw = 1;
                ph = 1;
            }
        }

        ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_decorated(GTK_WINDOW(ctx->window), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_accept_focus(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(ctx->window), (int) pw, (int) ph);
        gtk_widget_set_can_focus(GTK_WIDGET(ctx->window), TRUE);
        gtk_widget_add_events(GTK_WIDGET(ctx->window), GDK_BUTTON_PRESS_MASK);

        ctx->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        gtk_widget_set_can_focus(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_hexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_halign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        gtk_widget_set_valign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        gtk_widget_add_events(GTK_WIDGET(ctx->webview), GDK_BUTTON_PRESS_MASK);

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
        // 注意：不要用 gtk_widget_set_size_request() 强行指定 WebView 尺寸。
        // size_request 使用“逻辑像素”，在 HiDPI(scale factor>1) 下会导致实际渲染尺寸被放大，
        // 从而出现 WebView 比 AWT 宿主窗口更大、被裁剪且无法滚动的问题。

        gtk_container_add(GTK_CONTAINER(ctx->window), GTK_WIDGET(ctx->webview));
        gtk_widget_realize(ctx->window);

        ctx->child_xid = get_xid_from_gtk_window(ctx->window);
        if (ctx->child_xid == 0) {
            ok = false;
            return;
        }

        // 将 GTK 窗口 reparent 到 AWT drawable 内部，形成“嵌入式”渲染
        x11_ignore_errors([&] {
            // 设置 EWMH 属性：让这个用于嵌入的窗口不出现在 GNOME 概览/任务栏中。
            x11_set_ewmh_embed_hints(ctx->xdisplay, ctx->child_xid);

            XReparentWindow(ctx->xdisplay, ctx->child_xid, ctx->parent_xid, 0, 0);
            XMoveResizeWindow(ctx->xdisplay, ctx->child_xid, 0, 0, pw, ph);
            gtk_widget_show_all(ctx->window);
            XMapRaised(ctx->xdisplay, ctx->child_xid);
            XFlush(ctx->xdisplay);
        });
    });

    if (!ok) {
        wvbridge::gtk_run_on_thread_sync([&] { destroy_ctx_on_gtk_thread(ctx); });
        delete ctx;
        throw_jni_exception(env, "java/lang/RuntimeException", "Init WebView/attach to AWT failed (X11 only)");
        return 0;
    }

    // 2) 返回给 JVM 作为 handle
    return reinterpret_cast<jlong>(ctx);
}

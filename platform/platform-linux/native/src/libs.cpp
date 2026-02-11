#include <jawt.h>
#include <jawt_md.h>

#include <X11/Xlib.h>

#include <algorithm>
#include <atomic>

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include <gdk/x11/gdkx.h>

#include "gtk.h"
#include "utils.h"

struct WebViewContext {
    // AWT 侧宿主窗口 (X11 Window ID)
    ::Window parent_xid = 0;

    // GTK/WebKit 创建出的窗口 (被 reparent 到 parent_xid)
    ::Window child_xid = 0;

    // 使用 GTK(GDK) 默认 Display 对应的 Xlib Display (仅 X11 下有效)
    ::Display *xdisplay = nullptr;

    GtkWidget *window = nullptr;
    WebKitWebView *webview = nullptr;

    // 关闭流程开始后置 true；用于拒绝后续 update 排队，避免 close0 后异步访问已销毁对象。
    std::atomic_bool closing{false};
};

namespace {

    int clamp_dim(jint v) {
        return (v < 1) ? 1 : (int) v;
    }

    ::Display *get_xdisplay_from_gdk_default() {
        GdkDisplay *gdpy = gdk_display_get_default();
        if (!gdpy) return nullptr;

        // WebKitGTK 6.0 + JAWT_X11DrawingSurfaceInfo -> 仅支持 X11 嵌入
        if (!GDK_IS_X11_DISPLAY(gdpy)) return nullptr;

        return gdk_x11_display_get_xdisplay(gdpy);
    }

    ::Window get_xid_from_gtk_window(GtkWidget *w) {
        if (!w) return 0;

        auto *surface = gtk_native_get_surface(GTK_NATIVE(w));
        if (!surface) return 0;

        return (::Window) gdk_x11_surface_get_xid(surface);
    }

    // 在 X11 下吞掉由窗口已被外部销毁/重父子关系导致的 BadWindow 等错误，避免 GDK 打 warning。
    void x11_ignore_errors(const std::function<void()> &fn) {
        GdkDisplay *gdpy = gdk_display_get_default();
        if (gdpy && GDK_IS_X11_DISPLAY(gdpy)) {
            gdk_x11_display_error_trap_push(gdpy);
            if (fn) fn();
            gdk_x11_display_error_trap_pop_ignored(gdpy);
            return;
        }

        if (fn) fn();
    }

    void destroy_ctx_on_gtk_thread(WebViewContext *ctx) {
        if (!ctx) return;

        // GtkWindow 关闭后，会连带销毁子树（WebView）。
        if (ctx->window) {
            gtk_window_destroy(GTK_WINDOW(ctx->window));
            ctx->window = nullptr;
            ctx->webview = nullptr;
        }

        ctx->child_xid = 0;
        ctx->xdisplay = nullptr;
        ctx->parent_xid = 0;
    }

} // namespace

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

        ctx->window = gtk_window_new();
        gtk_window_set_decorated(GTK_WINDOW(ctx->window), FALSE);
        gtk_window_set_resizable(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_default_size(GTK_WINDOW(ctx->window), (int) pw, (int) ph);

        ctx->webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        gtk_widget_set_hexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(ctx->webview), TRUE);
        gtk_widget_set_halign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        gtk_widget_set_valign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
        // 注意：不要用 gtk_widget_set_size_request() 强行指定 WebView 尺寸。
        // size_request 使用“逻辑像素”，在 HiDPI(scale factor>1) 下会导致实际渲染尺寸被放大，
        // 从而出现 WebView 比 AWT 宿主窗口更大、被裁剪且无法滚动的问题。

        gtk_window_set_child(GTK_WINDOW(ctx->window), GTK_WIDGET(ctx->webview));
        // present 触发 map/realize，确保有 GdkSurface
        gtk_window_present(GTK_WINDOW(ctx->window));

        ctx->child_xid = get_xid_from_gtk_window(ctx->window);
        if (ctx->child_xid == 0) {
            ok = false;
            return;
        }

        // 将 GTK 窗口 reparent 到 AWT drawable 内部，形成“嵌入式”渲染
        x11_ignore_errors([&] {
            XReparentWindow(ctx->xdisplay, ctx->child_xid, ctx->parent_xid, 0, 0);
            XMoveResizeWindow(ctx->xdisplay, ctx->child_xid, 0, 0, pw, ph);
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
            // 让 WebView 跟随其顶层窗口(surface)的真实尺寸分配，而不是用 size_request 固定。
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

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (url == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        // OOM 或 JVM 已抛异常
        return;
    }

    // 必须同步：否则 close0 可能先 delete ctx，导致异步闭包访问悬空指针。
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (!ctx->webview) return;

        const char *uri = nativeString;
        if (!uri || uri[0] == '\0') uri = "about:blank";

        webkit_web_view_load_uri(ctx->webview, uri);
    });

    env->ReleaseStringUTFChars(url, nativeString);
}

//private external fun setProgressListener(webview: Long, consumer: Consumer<Float>)
API_EXPORT(void, setProgressListener, jlong handle, jobject listener) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    (void) listener;
}

//private external fun setNavigationHandler(webview: Long, handler: Function<String, Boolean>)
API_EXPORT(void, setNavigationHandler, jlong handle, jobject handler) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    (void) handler;
}

#include "libs_helpers.h"

#include <exception>
#include <memory>
#include <string>

#include <wvbridge/javascript.h>
#include <wvbridge/logger.h>
#include <wvbridge/webview-platform-settings.h>

#include "webview_lifecycle.h"
#include "x11_embed.h"

namespace {

class JawtSurfaceGuard {
public:
    bool acquire(JNIEnv* env, jobject component, std::string* error) {
        LOGGER_D("init.jawt: phase=acquire env=%p component=%p", env, component);
        awt_.version = JAWT_VERSION_1_4;
        if (JAWT_GetAWT(env, &awt_) == JNI_FALSE) {
            if (error) *error = "JAWT_GetAWT failed";
            LOGGER_E("init.jawt: JAWT_GetAWT failed version=0x%x", JAWT_VERSION_1_4);
            return false;
        }
        LOGGER_V("init.jawt: JAWT acquired GetDrawingSurface=%p FreeDrawingSurface=%p",
                 reinterpret_cast<void*>(awt_.GetDrawingSurface),
                 reinterpret_cast<void*>(awt_.FreeDrawingSurface));

        surface_ = awt_.GetDrawingSurface(env, component);
        if (!surface_) {
            if (error) *error = "JAWT GetDrawingSurface failed";
            LOGGER_E("init.jawt: GetDrawingSurface returned null component=%p", component);
            return false;
        }
        LOGGER_V("init.jawt: drawing surface acquired surface=%p", surface_);

        const jint lock_result = surface_->Lock(surface_);
        LOGGER_V("init.jawt: Lock returned flags=0x%x surface=%p", lock_result, surface_);
        if ((lock_result & JAWT_LOCK_ERROR) != 0) {
            if (error) *error = "JAWT drawing surface lock failed";
            LOGGER_E("init.jawt: drawing surface lock failed flags=0x%x", lock_result);
            return false;
        }
        locked_ = true;
        if ((lock_result & JAWT_LOCK_SURFACE_CHANGED) != 0) {
            LOGGER_D("init.jawt: lock reports surface changed; using freshly queried platform info");
        }
        if ((lock_result & JAWT_LOCK_BOUNDS_CHANGED) != 0) {
            LOGGER_V("init.jawt: lock reports bounds changed");
        }
        if ((lock_result & JAWT_LOCK_CLIP_CHANGED) != 0) {
            LOGGER_V("init.jawt: lock reports clip changed");
        }

        info_ = surface_->GetDrawingSurfaceInfo(surface_);
        if (!info_) {
            if (error) *error = "JAWT GetDrawingSurfaceInfo failed";
            LOGGER_E("init.jawt: GetDrawingSurfaceInfo returned null surface=%p", surface_);
            return false;
        }
        LOGGER_V("init.jawt: drawing info acquired info=%p platform_info=%p bounds=%d,%d %dx%d clip_count=%d",
                 info_, info_->platformInfo,
                 info_->bounds.x, info_->bounds.y, info_->bounds.width, info_->bounds.height,
                 info_->clipSize);
        return true;
    }

    JAWT_X11DrawingSurfaceInfo* x11_info() const {
        return info_ ? static_cast<JAWT_X11DrawingSurfaceInfo*>(info_->platformInfo) : nullptr;
    }

    void release() {
        LOGGER_V("init.jawt: release begin surface=%p info=%p locked=%d",
                 surface_, info_, locked_ ? 1 : 0);
        if (surface_ && info_) {
            surface_->FreeDrawingSurfaceInfo(info_);
            info_ = nullptr;
            LOGGER_V("init.jawt: drawing surface info freed");
        }
        if (surface_ && locked_) {
            surface_->Unlock(surface_);
            locked_ = false;
            LOGGER_V("init.jawt: drawing surface unlocked");
        }
        if (surface_) {
            awt_.FreeDrawingSurface(surface_);
            surface_ = nullptr;
            LOGGER_V("init.jawt: drawing surface freed");
        }
        LOGGER_D("init.jawt: phase=released");
    }

    ~JawtSurfaceGuard() { release(); }

private:
    JAWT awt_{};
    JAWT_DrawingSurface* surface_ = nullptr;
    JAWT_DrawingSurfaceInfo* info_ = nullptr;
    bool locked_ = false;
};

void set_failure(bool* ok, std::string* error, const std::string& message) {
    if (ok) *ok = false;
    if (error) *error = message;
    LOGGER_E("init.gtk: failure=%s", message.c_str());
}

WebKitWebView* create_webview(
    const WvBridgeLinuxWebViewPlatformSetting& setting,
    std::string* error
) {
    LOGGER_D("init.gtk: phase=create-webview data_dir_set=%d cache_dir_set=%d user_agent_set=%d",
             setting.data_dir.empty() ? 0 : 1,
             setting.cache_dir.empty() ? 0 : 1,
             setting.user_agent.empty() ? 0 : 1);
    WebKitWebView* webview = nullptr;
    if (setting.data_dir.empty() && setting.cache_dir.empty()) {
        webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
        LOGGER_V("init.gtk: default WebView created webview=%p", webview);
    } else {
        WebKitWebsiteDataManager* manager = nullptr;
        if (!setting.data_dir.empty() && !setting.cache_dir.empty()) {
            manager = webkit_website_data_manager_new(
                "base-data-directory", setting.data_dir.c_str(),
                "base-cache-directory", setting.cache_dir.c_str(),
                nullptr
            );
        } else if (!setting.data_dir.empty()) {
            manager = webkit_website_data_manager_new(
                "base-data-directory", setting.data_dir.c_str(), nullptr
            );
        } else {
            manager = webkit_website_data_manager_new(
                "base-cache-directory", setting.cache_dir.c_str(), nullptr
            );
        }
        LOGGER_V("init.gtk: website data manager created manager=%p", manager);
        if (!manager) {
            if (error) *error = "Unable to create WebKitWebsiteDataManager";
            LOGGER_E("init.gtk: website data manager creation returned null");
            return nullptr;
        }
        WebKitWebContext* web_context = webkit_web_context_new_with_website_data_manager(manager);
        LOGGER_V("init.gtk: WebKit context created context=%p manager=%p", web_context, manager);
        if (web_context) {
            webview = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(web_context));
            g_object_unref(web_context);
        }
        g_object_unref(manager);
        if (!web_context || !webview) {
            if (error) *error = "Unable to create WebKitWebContext/WebView";
            LOGGER_E("init.gtk: context or webview creation failed context=%p webview=%p",
                     web_context, webview);
            return nullptr;
        }
    }

    if (!webview) {
        if (error) *error = "Unable to create WebKitWebView";
        LOGGER_E("init.gtk: webkit_web_view_new returned null");
        return nullptr;
    }
    if (!setting.user_agent.empty()) {
        WebKitSettings* web_settings = webkit_web_view_get_settings(webview);
        LOGGER_V("init.gtk: applying user agent settings=%p length=%zu",
                 web_settings, setting.user_agent.size());
        if (web_settings) webkit_settings_set_user_agent(web_settings, setting.user_agent.c_str());
    }
    return webview;
}

void wvbridge_script_message_received(
    WebKitUserContentManager*,
    WebKitJavascriptResult* result,
    gpointer user_data
) {
    auto* ctx = static_cast<WebViewContext*>(user_data);
    LOGGER_V("webmessage.receive: entry ctx=%p result=%p closing=%d",
             ctx, result,
             ctx && ctx->closing.load(std::memory_order_acquire) ? 1 : 0);
    if (!ctx || !result) {
        LOGGER_W("webmessage.receive: missing context or result ctx=%p result=%p", ctx, result);
        return;
    }
    if (ctx->closing.load(std::memory_order_acquire)) {
        LOGGER_V("webmessage.receive: context closing; callback suppressed ctx=%p", ctx);
        return;
    }

    JSCValue* value = webkit_javascript_result_get_js_value(result);
    if (!value || jsc_value_is_undefined(value) || jsc_value_is_null(value)) {
        LOGGER_D("webmessage.receive: phase=dispatch-empty ctx=%p value=%p", ctx, value);
        wvbridge::dispatch_web_message_to_java(
            ctx->web_message_handlers_mutex, ctx->web_message_handlers, ""
        );
        LOGGER_V("webmessage.receive: empty message dispatched ctx=%p", ctx);
        return;
    }

    gchar* string_value = jsc_value_to_string(value);
    std::string message = string_value ? string_value : "";
    LOGGER_D("webmessage.receive: phase=dispatch ctx=%p bytes=%zu preview=%.100s",
             ctx, message.size(), message.c_str());
    if (string_value) g_free(string_value);
    wvbridge::dispatch_web_message_to_java(
        ctx->web_message_handlers_mutex, ctx->web_message_handlers, message.c_str()
    );
    LOGGER_V("webmessage.receive: dispatch complete ctx=%p bytes=%zu", ctx, message.size());
}

} // namespace

API_EXPORT(jlong, initAndAttach, jobject platformSetting) {
    LOGGER_I("init: begin env=%p component=%p platform_setting=%p", env, thiz, platformSetting);
    if (wvbridge::lifecycle_shutdown_requested()) {
        LOGGER_W("init: rejected because JVM shutdown is in progress");
        throw_jni_exception(env, "java/lang/IllegalStateException", "JVM shutdown is in progress");
        return 0;
    }

    WvBridgeLinuxWebViewPlatformSetting setting;
    LOGGER_D("init: phase=parse-platform-settings");
    if (!parse_webview_platform_settings(env, platformSetting, &setting)) {
        LOGGER_E("init: platform settings parsing failed setting=%p", platformSetting);
        return 0;
    }
    LOGGER_V("init: settings parsed data_dir_len=%zu cache_dir_len=%zu user_agent_len=%zu",
             setting.data_dir.size(), setting.cache_dir.size(), setting.user_agent.size());

    LOGGER_D("init: phase=start-gtk-runtime");
    if (!wvbridge::gtk_init()) {
        LOGGER_E("init: GTK runtime failed to start");
        throw_jni_exception(env, "java/lang/RuntimeException", "Unable to start GTK runtime");
        return 0;
    }

    std::string error;
    auto ctx = std::make_unique<WebViewContext>();
    const jlong handle = reinterpret_cast<jlong>(ctx.get());
    bool created = true;
    LOGGER_D("init: phase=create-on-gtk-thread ctx=%p handle=%lld",
             ctx.get(), static_cast<long long>(handle));

    try {
        const bool dispatched = wvbridge::gtk_run_on_thread_sync([&] {
            LOGGER_V("init.gtk: entry ctx=%p handle=%lld gtk_thread=%d",
                     ctx.get(), static_cast<long long>(handle),
                     wvbridge::gtk_is_gtk_thread() ? 1 : 0);

            ctx->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
            LOGGER_V("init.gtk: GtkWindow created window=%p", ctx->window);
            if (!ctx->window) {
                set_failure(&created, &error, "Unable to create GtkWindow");
                return;
            }
            gtk_window_set_decorated(GTK_WINDOW(ctx->window), FALSE);
            gtk_window_set_resizable(GTK_WINDOW(ctx->window), TRUE);
            gtk_window_set_accept_focus(GTK_WINDOW(ctx->window), TRUE);
            gtk_window_set_skip_taskbar_hint(GTK_WINDOW(ctx->window), TRUE);
            gtk_window_set_skip_pager_hint(GTK_WINDOW(ctx->window), TRUE);
            gtk_window_set_type_hint(GTK_WINDOW(ctx->window), GDK_WINDOW_TYPE_HINT_UTILITY);
            // The final size is applied after the locked JAWT surface has been
            // inspected. Avoid holding the AWT surface lock during WebKit init.
            gtk_window_set_default_size(GTK_WINDOW(ctx->window), 1, 1);
            gtk_widget_set_can_focus(ctx->window, TRUE);
            gtk_widget_add_events(ctx->window, GDK_BUTTON_PRESS_MASK);

            ctx->webview = create_webview(setting, &error);
            if (!ctx->webview) {
                set_failure(&created, &error, error.empty() ? "Unable to create WebKitWebView" : error);
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
                return;
            }
            gtk_widget_set_can_focus(GTK_WIDGET(ctx->webview), TRUE);
            gtk_widget_set_hexpand(GTK_WIDGET(ctx->webview), TRUE);
            gtk_widget_set_vexpand(GTK_WIDGET(ctx->webview), TRUE);
            gtk_widget_set_halign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
            gtk_widget_set_valign(GTK_WIDGET(ctx->webview), GTK_ALIGN_FILL);
            gtk_widget_add_events(GTK_WIDGET(ctx->webview), GDK_BUTTON_PRESS_MASK);
            gtk_container_add(GTK_CONTAINER(ctx->window), GTK_WIDGET(ctx->webview));
            LOGGER_V("init.gtk: WebView added to GtkWindow window=%p webview=%p",
                     ctx->window, ctx->webview);

            WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(ctx->webview);
            if (!manager || !webkit_user_content_manager_register_script_message_handler(manager, "wvbridge")) {
                set_failure(&created, &error, "Unable to register WebKit script message handler");
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
                return;
            }
            ctx->web_message_handler_id = g_signal_connect(
                manager, "script-message-received::wvbridge",
                G_CALLBACK(wvbridge_script_message_received), ctx.get()
            );
            ctx->window_button_press_handler_id = g_signal_connect(
                ctx->window, "button-press-event",
                G_CALLBACK(focus_on_button_press_cb), ctx.get()
            );
            ctx->webview_button_press_handler_id = g_signal_connect(
                ctx->webview, "button-press-event",
                G_CALLBACK(focus_on_button_press_cb), ctx.get()
            );
            LOGGER_V("init.gtk: signals connected message=%lu window_press=%lu webview_press=%lu",
                     static_cast<unsigned long>(ctx->web_message_handler_id),
                     static_cast<unsigned long>(ctx->window_button_press_handler_id),
                     static_cast<unsigned long>(ctx->webview_button_press_handler_id));

            ctx->events = wvbridge::webview_events_create(ctx->webview, handle, &ctx->closing);
            if (!ctx->events) {
                set_failure(&created, &error, "Unable to create WebView event bridge");
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
                return;
            }

            LOGGER_D("init.gtk: phase=realize-window ctx=%p window=%p", ctx.get(), ctx->window);
            gtk_widget_realize(ctx->window);
            if (!gtk_widget_get_realized(ctx->window)) {
                set_failure(&created, &error, "GtkWindow realization failed");
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
                return;
            }

            LOGGER_I("init.gtk: WebView creation and realization complete ctx=%p window=%p webview=%p",
                     ctx.get(), ctx->window, ctx->webview);
        });
        if (!dispatched) {
            created = false;
            error = "GTK runtime stopped before WebView creation could run";
            LOGGER_E("init: GTK runtime rejected synchronous creation task");
        }
    } catch (const std::exception& exception) {
        created = false;
        error = std::string("GTK creation threw: ") + exception.what();
        LOGGER_E("init: GTK creation exception=%s", exception.what());
    } catch (...) {
        created = false;
        error = "GTK creation threw an unknown exception";
        LOGGER_E("init: GTK creation threw unknown exception");
    }

    auto cleanup_unregistered_context = [&] {
        LOGGER_D("init: phase=cleanup-unregistered-context ctx=%p", ctx.get());
        ctx->closing.store(true, std::memory_order_release);
        const bool dispatched = wvbridge::gtk_run_on_thread_sync([&] {
            wvbridge::destroy_webview_on_gtk_thread(ctx.get());
        });
        if (!dispatched) {
            LOGGER_E("init: GTK runtime rejected cleanup for unregistered context ctx=%p", ctx.get());
        }
    };

    if (!created || !ctx->window || !ctx->webview) {
        LOGGER_E("init: GTK creation failed created=%d window=%p webview=%p error=%s",
                 created ? 1 : 0, ctx->window, ctx->webview, error.c_str());
        if (ctx->window || ctx->webview) cleanup_unregistered_context();
        throw_jni_exception(
            env, "java/lang/RuntimeException",
            error.empty() ? "Unable to initialize the Linux WebView" : error.c_str()
        );
        return 0;
    }

    JawtSurfaceGuard jawt;
    LOGGER_D("init: phase=lock-awt-surface-for-attach ctx=%p", ctx.get());
    error.clear();
    if (!jawt.acquire(env, thiz, &error)) {
        LOGGER_E("init: JAWT surface acquisition failed error=%s", error.c_str());
        cleanup_unregistered_context();
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
        return 0;
    }

    JAWT_X11DrawingSurfaceInfo* xinfo = jawt.x11_info();
    if (!xinfo) {
        LOGGER_E("init: JAWT platformInfo is null or not X11");
        jawt.release();
        cleanup_unregistered_context();
        throw_jni_exception(env, "java/lang/RuntimeException", "JAWT did not provide X11 surface information");
        return 0;
    }
    LOGGER_V("init: JAWT X11 info=%p display=%p drawable=%lu visual=%lu colormap=%lu depth=%d",
             xinfo, xinfo->display, static_cast<unsigned long>(xinfo->drawable),
             static_cast<unsigned long>(xinfo->visualID),
             static_cast<unsigned long>(xinfo->colormapID), xinfo->depth);

    wvbridge::AwtX11Surface parent;
    LOGGER_D("init: phase=inspect-awt-x11-surface");
    if (!wvbridge::inspect_awt_x11_surface(xinfo->display, xinfo->drawable, &parent, &error)) {
        LOGGER_E("init: AWT X11 surface inspection failed error=%s", error.c_str());
        jawt.release();
        cleanup_unregistered_context();
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
        return 0;
    }

    bool attached = false;
    LOGGER_D("init: phase=attach-on-gtk-thread-with-jawt-locked ctx=%p parent=%lu",
             ctx.get(), static_cast<unsigned long>(parent.drawable));
    try {
        const bool dispatched = wvbridge::gtk_run_on_thread_sync([&] {
            LOGGER_V("init.gtk.attach: entry ctx=%p parent=%lu gtk_thread=%d",
                     ctx.get(), static_cast<unsigned long>(parent.drawable),
                     wvbridge::gtk_is_gtk_thread() ? 1 : 0);
            attached = wvbridge::attach_gtk_window_to_awt(ctx.get(), parent, &error);
            if (!attached) {
                LOGGER_E("init.gtk.attach: X11 attach failed ctx=%p error=%s", ctx.get(), error.c_str());
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
                return;
            }
            LOGGER_I("init.gtk.attach: complete ctx=%p child=%lu parent=%lu",
                     ctx.get(), static_cast<unsigned long>(ctx->child_xid),
                     static_cast<unsigned long>(ctx->parent_xid));
        });
        if (!dispatched) {
            attached = false;
            error = "GTK runtime stopped before X11 attach could run";
            LOGGER_E("init: GTK runtime rejected X11 attach task ctx=%p", ctx.get());
        }
    } catch (const std::exception& exception) {
        attached = false;
        error = std::string("X11 attach threw: ") + exception.what();
        LOGGER_E("init: X11 attach exception=%s", exception.what());
    } catch (...) {
        attached = false;
        error = "X11 attach threw an unknown exception";
        LOGGER_E("init: X11 attach threw unknown exception");
    }

    LOGGER_D("init: phase=release-awt-surface-after-attach attached=%d ctx=%p",
             attached ? 1 : 0, ctx.get());
    jawt.release();
    if (!attached || !ctx->attached.load(std::memory_order_acquire)) {
        LOGGER_E("init: attach failed attached=%d context_attached=%d error=%s",
                 attached ? 1 : 0,
                 ctx->attached.load(std::memory_order_acquire) ? 1 : 0,
                 error.c_str());
        // attach_gtk_window_to_awt cleans up on ordinary failure. This covers
        // dispatch/exception paths where the GTK closure did not complete.
        if (ctx->window || ctx->webview) cleanup_unregistered_context();
        throw_jni_exception(
            env, "java/lang/RuntimeException",
            error.empty() ? "Unable to attach the Linux WebView to AWT" : error.c_str()
        );
        return 0;
    }

    LOGGER_D("init: phase=register-lifecycle ctx=%p handle=%lld", ctx.get(), static_cast<long long>(handle));
    if (!wvbridge::lifecycle_register(ctx.get())) {
        LOGGER_W("init: lifecycle registration rejected; destroying newly attached context ctx=%p", ctx.get());
        ctx->closing.store(true, std::memory_order_release);
        if (!wvbridge::gtk_run_on_thread_sync([&] {
                wvbridge::destroy_webview_on_gtk_thread(ctx.get());
            })) {
            LOGGER_E("init: unable to dispatch cleanup after lifecycle registration rejection ctx=%p", ctx.get());
        }
        throw_jni_exception(env, "java/lang/IllegalStateException", "JVM shutdown started during WebView initialization");
        return 0;
    }

    ctx.release();
    LOGGER_I("init: success handle=%lld child=%lu parent=%lu",
             static_cast<long long>(handle),
             static_cast<unsigned long>(reinterpret_cast<WebViewContext*>(handle)->child_xid),
             static_cast<unsigned long>(reinterpret_cast<WebViewContext*>(handle)->parent_xid));
    return handle;
}

#include "webview_lifecycle.h"

#include <mutex>
#include <unordered_set>
#include <vector>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <wvbridge/javascript.h>
#include <wvbridge/logger.h>

#include "gtk.h"
#include "webview_context.h"
#include "webview_events.h"

namespace wvbridge {
namespace {

std::mutex g_lifecycle_mutex;
std::unordered_set<WebViewContext*> g_active_contexts;
std::size_t g_closing_contexts = 0;
bool g_shutdown_requested = false;

void disconnect_signal_if_present(gpointer instance, gulong& handler_id, const char* semantic_name) {
    LOGGER_V("webview.destroy.signal: name=%s instance=%p handler_id=%lu",
             semantic_name, instance, static_cast<unsigned long>(handler_id));
    if (instance && handler_id != 0) {
        g_signal_handler_disconnect(instance, handler_id);
        LOGGER_V("webview.destroy.signal: disconnected name=%s handler_id=%lu",
                 semantic_name, static_cast<unsigned long>(handler_id));
    }
    handler_id = 0;
}

} // namespace

bool lifecycle_register(WebViewContext* ctx) {
    LOGGER_I("lifecycle.register: ctx=%p", ctx);
    if (!ctx) {
        LOGGER_E("lifecycle.register: null context");
        return false;
    }
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (g_shutdown_requested) {
        LOGGER_W("lifecycle.register: rejected because JVM shutdown was requested ctx=%p active=%zu closing=%zu",
                 ctx, g_active_contexts.size(), g_closing_contexts);
        return false;
    }
    const bool inserted = g_active_contexts.insert(ctx).second;
    if (!inserted) {
        LOGGER_E("lifecycle.register: duplicate context ctx=%p active=%zu", ctx, g_active_contexts.size());
        return false;
    }
    LOGGER_D("lifecycle.register: registered ctx=%p active=%zu closing=%zu",
             ctx, g_active_contexts.size(), g_closing_contexts);
    return true;
}

CloseTicket lifecycle_begin_close(WebViewContext* ctx, bool jvm_exit_progress) {
    LOGGER_I("lifecycle.close.begin: ctx=%p jvm_exit=%d", ctx, jvm_exit_progress ? 1 : 0);
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (jvm_exit_progress) {
        g_shutdown_requested = true;
        LOGGER_D("lifecycle.close.begin: global shutdown flag set active=%zu closing=%zu",
                 g_active_contexts.size(), g_closing_contexts);
    }

    CloseTicket ticket;
    ticket.shutdown_requested = g_shutdown_requested;
    if (ctx) {
        const auto it = g_active_contexts.find(ctx);
        if (it != g_active_contexts.end()) {
            g_active_contexts.erase(it);
            ++g_closing_contexts;
            ticket.owns_context = true;
            ctx->closing.store(true, std::memory_order_release);
            LOGGER_D("lifecycle.close.begin: context claimed ctx=%p active=%zu closing=%zu",
                     ctx, g_active_contexts.size(), g_closing_contexts);
        } else {
            LOGGER_W("lifecycle.close.begin: unknown or already closing context ctx=%p active=%zu closing=%zu",
                     ctx, g_active_contexts.size(), g_closing_contexts);
        }
    } else {
        LOGGER_V("lifecycle.close.begin: no context supplied; shutdown notification only");
    }
    ticket.active_contexts = g_active_contexts.size();
    ticket.closing_contexts = g_closing_contexts;
    return ticket;
}

bool lifecycle_finish_close(const CloseTicket& ticket) {
    LOGGER_I("lifecycle.close.finish: owns=%d shutdown=%d begin_active=%zu begin_closing=%zu",
             ticket.owns_context ? 1 : 0, ticket.shutdown_requested ? 1 : 0,
             ticket.active_contexts, ticket.closing_contexts);
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    if (ticket.owns_context) {
        if (g_closing_contexts == 0) {
            LOGGER_E("lifecycle.close.finish: closing counter underflow prevented");
        } else {
            --g_closing_contexts;
        }
    }
    const bool stop_gtk = g_shutdown_requested &&
                           g_active_contexts.empty() &&
                           g_closing_contexts == 0;
    LOGGER_D("lifecycle.close.finish: active=%zu closing=%zu shutdown=%d stop_gtk=%d",
             g_active_contexts.size(), g_closing_contexts,
             g_shutdown_requested ? 1 : 0, stop_gtk ? 1 : 0);
    return stop_gtk;
}

bool lifecycle_shutdown_requested() {
    std::lock_guard<std::mutex> lock(g_lifecycle_mutex);
    LOGGER_V("lifecycle.shutdown.query: requested=%d active=%zu closing=%zu",
             g_shutdown_requested ? 1 : 0, g_active_contexts.size(), g_closing_contexts);
    return g_shutdown_requested;
}

bool destroy_webview_on_gtk_thread(WebViewContext* ctx) {
    LOGGER_I("webview.destroy: begin ctx=%p gtk_thread=%d", ctx, gtk_is_gtk_thread() ? 1 : 0);
    if (!ctx) {
        LOGGER_E("webview.destroy: null context");
        return false;
    }
    if (!gtk_is_gtk_thread()) {
        LOGGER_E("webview.destroy: rejected off GTK thread ctx=%p", ctx);
        return false;
    }

    LOGGER_D("webview.destroy: phase=disconnect-callbacks ctx=%p window=%p webview=%p events=%p",
             ctx, ctx->window, ctx->webview, ctx->events);
    disconnect_signal_if_present(ctx->window, ctx->window_button_press_handler_id, "window-button-press");
    disconnect_signal_if_present(ctx->webview, ctx->webview_button_press_handler_id, "webview-button-press");

    if (ctx->webview) {
        WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(ctx->webview);
        LOGGER_V("webview.destroy: user-content-manager=%p message_handler_id=%lu",
                 manager, static_cast<unsigned long>(ctx->web_message_handler_id));
        disconnect_signal_if_present(manager, ctx->web_message_handler_id, "script-message-received");
        if (manager) {
            webkit_user_content_manager_unregister_script_message_handler(manager, "wvbridge");
            LOGGER_V("webview.destroy: script message handler unregistered name=wvbridge");
        }
    } else {
        ctx->web_message_handler_id = 0;
    }

    if (ctx->events) {
        LOGGER_V("webview.destroy: destroying event bridge events=%p", ctx->events);
        webview_events_destroy(ctx->events);
        ctx->events = nullptr;
    }

    LOGGER_D("webview.destroy: phase=release-webkit-resources hooks=%zu webview=%p",
             ctx->document_start_hooks.size(), ctx->webview);
    if (ctx->webview) {
        webkit_web_view_stop_loading(ctx->webview);
        WebKitUserContentManager* manager = webkit_web_view_get_user_content_manager(ctx->webview);
        for (const auto& entry : ctx->document_start_hooks) {
            LOGGER_V("webview.destroy: removing document hook id=%lld script=%p",
                     static_cast<long long>(entry.first), entry.second);
            if (manager && entry.second) webkit_user_content_manager_remove_script(manager, entry.second);
            if (entry.second) webkit_user_script_unref(entry.second);
        }
    }
    ctx->document_start_hooks.clear();

    LOGGER_D("webview.destroy: phase=destroy-widget window=%p child_xid=%lu parent_xid=%lu attached=%d",
             ctx->window, static_cast<unsigned long>(ctx->child_xid),
             static_cast<unsigned long>(ctx->parent_xid),
             ctx->attached.load(std::memory_order_acquire) ? 1 : 0);
    if (ctx->window) {
        gtk_widget_destroy(ctx->window);
        LOGGER_V("webview.destroy: gtk_widget_destroy returned window=%p", ctx->window);
    }
    ctx->window = nullptr;
    ctx->webview = nullptr;
    ctx->attached.store(false, std::memory_order_release);
    ctx->child_xid = 0;

    if (ctx->foreign_parent_window) {
        LOGGER_V("webview.destroy: unref foreign parent wrapper=%p xid=%lu",
                 ctx->foreign_parent_window, static_cast<unsigned long>(ctx->parent_xid));
        g_object_unref(ctx->foreign_parent_window);
        ctx->foreign_parent_window = nullptr;
    }
    ctx->parent_xid = 0;
    ctx->xdisplay = nullptr;
    ctx->gdk_display = nullptr;
    LOGGER_I("webview.destroy: complete ctx=%p", ctx);
    return true;
}

void release_context_jvm_references(JNIEnv* env, WebViewContext* ctx) {
    LOGGER_I("webview.jvm_refs.release: begin env=%p ctx=%p", env, ctx);
    if (!ctx) {
        LOGGER_E("webview.jvm_refs.release: null context");
        return;
    }
    if (!env) {
        std::lock_guard<std::mutex> lock(ctx->web_message_handlers_mutex);
        LOGGER_W("webview.jvm_refs.release: env unavailable; leaking %zu global refs to avoid JVM attach during shutdown",
                 ctx->web_message_handlers.size());
        ctx->web_message_handlers.clear();
        return;
    }
    delete_web_message_handler_refs(env, ctx->web_message_handlers_mutex, ctx->web_message_handlers);
    LOGGER_I("webview.jvm_refs.release: complete ctx=%p", ctx);
}

} // namespace wvbridge

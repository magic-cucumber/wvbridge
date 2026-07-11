#include "libs_helpers.h"

#include <exception>

#include <wvbridge/logger.h>

#include "webview_lifecycle.h"

API_EXPORT(void, close0, jlong handle, jboolean isInJvmExitProgress) {
    (void) thiz;
    const bool jvm_exit = isInJvmExitProgress == JNI_TRUE;
    auto* ctx = reinterpret_cast<WebViewContext*>(handle);
    LOGGER_I("close: begin env=%p handle=%lld ctx=%p jvm_exit=%d gtk_initialized=%d",
             env, static_cast<long long>(handle), ctx, jvm_exit ? 1 : 0,
             wvbridge::gtk_is_inited() ? 1 : 0);

    LOGGER_D("close: phase=claim-context handle=%lld jvm_exit=%d",
             static_cast<long long>(handle), jvm_exit ? 1 : 0);
    const wvbridge::CloseTicket ticket = wvbridge::lifecycle_begin_close(ctx, jvm_exit);
    LOGGER_V("close: ticket owns=%d shutdown=%d active=%zu closing=%zu",
             ticket.owns_context ? 1 : 0, ticket.shutdown_requested ? 1 : 0,
             ticket.active_contexts, ticket.closing_contexts);

    bool gtk_destroyed = false;
    if (ticket.owns_context) {
        LOGGER_D("close: phase=destroy-on-gtk-thread ctx=%p child=%lu parent=%lu attached=%d",
                 ctx, static_cast<unsigned long>(ctx->child_xid),
                 static_cast<unsigned long>(ctx->parent_xid),
                 ctx->attached.load(std::memory_order_acquire) ? 1 : 0);
        try {
            const bool dispatched = wvbridge::gtk_run_on_thread_sync([&] {
                LOGGER_V("close.gtk: destroy task entered ctx=%p gtk_thread=%d closing=%d",
                         ctx, wvbridge::gtk_is_gtk_thread() ? 1 : 0,
                         ctx->closing.load(std::memory_order_acquire) ? 1 : 0);
                gtk_destroyed = wvbridge::destroy_webview_on_gtk_thread(ctx);
                LOGGER_V("close.gtk: destroy task completed ctx=%p destroyed=%d",
                         ctx, gtk_destroyed ? 1 : 0);
            });
            if (!dispatched) {
                LOGGER_E("close: GTK runtime rejected destroy task ctx=%p jvm_exit=%d",
                         ctx, jvm_exit ? 1 : 0);
            }
        } catch (const std::exception& exception) {
            LOGGER_E("close: GTK destroy threw std::exception ctx=%p error=%s",
                     ctx, exception.what());
        } catch (...) {
            LOGGER_E("close: GTK destroy threw unknown exception ctx=%p", ctx);
        }

        LOGGER_D("close: phase=release-jvm-global-refs ctx=%p gtk_destroyed=%d env=%p",
                 ctx, gtk_destroyed ? 1 : 0, env);
        // This deliberately runs on the ShutdownHook/native caller thread. The
        // GTK thread must never attach to a VM that is already shutting down.
        wvbridge::release_context_jvm_references(env, ctx);

        LOGGER_D("close: phase=delete-native-context ctx=%p gtk_destroyed=%d",
                 ctx, gtk_destroyed ? 1 : 0);
        if (gtk_destroyed) {
            delete ctx;
            ctx = nullptr;
            LOGGER_V("close: native context deleted handle=%lld", static_cast<long long>(handle));
        } else {
            // A still-live GObject may retain ctx as signal user_data. Leaking
            // this tiny native holder is safer than introducing a shutdown UAF;
            // ordinary operation never reaches this branch.
            LOGGER_E("close: native context intentionally retained because GTK destruction did not complete ctx=%p handle=%lld",
                     ctx, static_cast<long long>(handle));
        }
    } else if (handle == 0) {
        if (jvm_exit) {
            LOGGER_V("close: shutdown notification has no live handle; lifecycle will still evaluate GTK stop");
        } else {
            LOGGER_W("close: normal close received null handle; nothing to destroy");
        }
    } else {
        LOGGER_W("close: stale, duplicate, or foreign handle ignored handle=%lld ctx=%p",
                 static_cast<long long>(handle), ctx);
    }

    LOGGER_D("close: phase=finish-lifecycle handle=%lld", static_cast<long long>(handle));
    const bool stop_gtk = wvbridge::lifecycle_finish_close(ticket);
    LOGGER_V("close: lifecycle finished handle=%lld stop_gtk=%d gtk_destroyed=%d",
             static_cast<long long>(handle), stop_gtk ? 1 : 0, gtk_destroyed ? 1 : 0);

    if (stop_gtk) {
        LOGGER_I("close: phase=stop-gtk-runtime reason=jvm-exit-all-contexts-closed");
        wvbridge::gtk_stop();
        LOGGER_I("close: GTK runtime stopped and joined");
    }

    LOGGER_I("close: complete handle=%lld jvm_exit=%d owned=%d gtk_destroyed=%d stopped_gtk=%d",
             static_cast<long long>(handle), jvm_exit ? 1 : 0,
             ticket.owns_context ? 1 : 0, gtk_destroyed ? 1 : 0, stop_gtk ? 1 : 0);

    if (jvm_exit && stop_gtk) {
        // JNI_OnUnload is executed by HotSpot's VM Thread while VM_Exit owns a
        // safepoint. Joining a logger thread that is attached to the JVM from
        // there deadlocks: the VM Thread waits in pthread_join while the logger
        // waits for the safepoint to end. Stop and join it here instead, while
        // this Java shutdown-hook thread is still allowed to execute Java/JNI.
        // logger_on_unload() is idempotent, so the later JNI_OnUnload call is a
        // non-blocking no-op. No LOGGER_* call is allowed after this point.
        logger_on_unload();
    }
}

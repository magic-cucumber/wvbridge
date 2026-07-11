#include "gtk.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <wvbridge/logger.h>

namespace wvbridge {
namespace {

enum class GtkRuntimeState {
    stopped,
    starting,
    running,
    stopping,
};

std::mutex g_runtime_mutex;
std::condition_variable g_runtime_changed;
GtkRuntimeState g_state = GtkRuntimeState::stopped;
std::thread g_gtk_thread;
std::thread::id g_gtk_thread_id;
GMainLoop* g_loop = nullptr;
GMainContext* g_context = nullptr;
bool g_gtk_initialized = false;

const char* state_name(GtkRuntimeState state) {
    switch (state) {
        case GtkRuntimeState::stopped: return "stopped";
        case GtkRuntimeState::starting: return "starting";
        case GtkRuntimeState::running: return "running";
        case GtkRuntimeState::stopping: return "stopping";
    }
    return "unknown";
}

struct AsyncCall {
    std::function<void()> fn;
};

gboolean run_async_call(gpointer data) {
    auto* call = static_cast<AsyncCall*>(data);
    LOGGER_V("gtk.invoke.async.dispatch: call=%p has_fn=%d", call, call && call->fn ? 1 : 0);
    try {
        if (call && call->fn) call->fn();
    } catch (const std::exception& error) {
        LOGGER_E("gtk.invoke.async.dispatch: callback threw std::exception=%s", error.what());
    } catch (...) {
        LOGGER_E("gtk.invoke.async.dispatch: callback threw unknown exception");
    }
    LOGGER_V("gtk.invoke.async.complete: call=%p", call);
    return G_SOURCE_REMOVE;
}

void destroy_async_call(gpointer data) {
    LOGGER_V("gtk.invoke.async.release: call=%p", data);
    delete static_cast<AsyncCall*>(data);
}

struct SyncCall {
    std::function<void()> fn;
    std::mutex mutex;
    std::condition_variable changed;
    bool done = false;
    std::exception_ptr exception;
};

gboolean run_sync_call(gpointer data) {
    auto* call = static_cast<SyncCall*>(data);
    LOGGER_V("gtk.invoke.sync.dispatch: call=%p has_fn=%d", call, call && call->fn ? 1 : 0);
    try {
        if (call && call->fn) call->fn();
    } catch (...) {
        if (call) call->exception = std::current_exception();
        LOGGER_E("gtk.invoke.sync.dispatch: callback threw; exception captured for caller");
    }
    if (call) {
        std::lock_guard<std::mutex> lock(call->mutex);
        call->done = true;
        call->changed.notify_all();
    }
    LOGGER_V("gtk.invoke.sync.complete: call=%p", call);
    return G_SOURCE_REMOVE;
}

gboolean quit_loop(gpointer data) {
    auto* loop = static_cast<GMainLoop*>(data);
    LOGGER_D("gtk.stop.dispatch: loop=%p thread_is_gtk=%d", loop,
             std::this_thread::get_id() == g_gtk_thread_id ? 1 : 0);
    if (loop) g_main_loop_quit(loop);
    LOGGER_V("gtk.stop.dispatch: quit requested loop=%p", loop);
    return G_SOURCE_REMOVE;
}

bool attach_idle_source_locked(GSourceFunc callback, gpointer data, GDestroyNotify destroy) {
    if (g_state != GtkRuntimeState::running || g_context == nullptr) {
        LOGGER_W("gtk.source.attach: runtime unavailable state=%s context=%p callback=%p",
                 state_name(g_state), g_context, reinterpret_cast<void*>(callback));
        return false;
    }

    GSource* source = g_idle_source_new();
    if (!source) {
        LOGGER_E("gtk.source.attach: g_idle_source_new returned null callback=%p",
                 reinterpret_cast<void*>(callback));
        return false;
    }
    g_source_set_priority(source, G_PRIORITY_DEFAULT);
    g_source_set_callback(source, callback, data, destroy);
    const guint id = g_source_attach(source, g_context);
    g_source_unref(source);
    if (id == 0) {
        LOGGER_E("gtk.source.attach: g_source_attach failed callback=%p context=%p",
                 reinterpret_cast<void*>(callback), g_context);
        return false;
    }
    LOGGER_V("gtk.source.attach: source_id=%u callback=%p data=%p context=%p",
             id, reinterpret_cast<void*>(callback), data, g_context);
    g_main_context_wakeup(g_context);
    return true;
}

} // namespace

bool gtk_is_gtk_thread() {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    const bool result = g_state != GtkRuntimeState::stopped &&
                        std::this_thread::get_id() == g_gtk_thread_id;
    LOGGER_V("gtk.thread.check: result=%d state=%s", result ? 1 : 0, state_name(g_state));
    return result;
}

bool gtk_is_inited() {
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    const bool result = g_state == GtkRuntimeState::running && g_loop != nullptr && g_context != nullptr;
    LOGGER_V("gtk.runtime.query: initialized=%d state=%s loop=%p context=%p",
             result ? 1 : 0, state_name(g_state), g_loop, g_context);
    return result;
}

bool gtk_init() {
    LOGGER_I("gtk.runtime.start: requested");
    std::unique_lock<std::mutex> lock(g_runtime_mutex);
    while (g_state == GtkRuntimeState::starting || g_state == GtkRuntimeState::stopping) {
        LOGGER_V("gtk.runtime.start: waiting for transition state=%s", state_name(g_state));
        g_runtime_changed.wait(lock);
    }
    if (g_state == GtkRuntimeState::running) {
        LOGGER_D("gtk.runtime.start: already running thread_id_hash=%zu",
                 std::hash<std::thread::id>{}(g_gtk_thread_id));
        return true;
    }
    if (g_gtk_thread.joinable()) {
        LOGGER_E("gtk.runtime.start: stopped state owns joinable thread; refusing unsafe restart");
        return false;
    }

    g_state = GtkRuntimeState::starting;
    LOGGER_D("gtk.runtime.start: creating dedicated GTK thread");
    try {
        g_gtk_thread = std::thread([] {
            {
                std::lock_guard<std::mutex> thread_lock(g_runtime_mutex);
                g_gtk_thread_id = std::this_thread::get_id();
            }
            LOGGER_D("gtk.runtime.thread: entered thread_id_hash=%zu",
                     std::hash<std::thread::id>{}(std::this_thread::get_id()));

            bool initialized = true;
            if (!g_gtk_initialized) {
                LOGGER_D("gtk.runtime.thread: calling gtk_init_check");
                initialized = ::gtk_init_check(nullptr, nullptr) != FALSE;
                if (initialized) g_gtk_initialized = true;
            } else {
                LOGGER_D("gtk.runtime.thread: GTK library already initialized; creating a new main-loop owner");
            }

            GMainContext* context = initialized ? g_main_context_default() : nullptr;
            GMainLoop* loop = context ? g_main_loop_new(context, FALSE) : nullptr;
            {
                std::lock_guard<std::mutex> thread_lock(g_runtime_mutex);
                if (!initialized || !loop) {
                    g_context = nullptr;
                    g_loop = nullptr;
                    g_state = GtkRuntimeState::stopped;
                    LOGGER_E("gtk.runtime.thread: initialization failed initialized=%d loop=%p",
                             initialized ? 1 : 0, loop);
                } else {
                    g_context = context;
                    g_loop = loop;
                    g_state = GtkRuntimeState::running;
                    LOGGER_I("gtk.runtime.thread: running loop=%p context=%p", loop, context);
                }
                g_runtime_changed.notify_all();
            }

            if (initialized && loop) {
                LOGGER_V("gtk.runtime.thread: entering g_main_loop_run loop=%p", loop);
                g_main_loop_run(loop);
                LOGGER_D("gtk.runtime.thread: g_main_loop_run returned loop=%p", loop);
                g_main_loop_unref(loop);
            }

            {
                std::lock_guard<std::mutex> thread_lock(g_runtime_mutex);
                g_loop = nullptr;
                g_context = nullptr;
                g_gtk_thread_id = {};
                g_state = GtkRuntimeState::stopped;
                g_runtime_changed.notify_all();
            }
            LOGGER_I("gtk.runtime.thread: exited cleanly");
        });
    } catch (const std::exception& error) {
        g_state = GtkRuntimeState::stopped;
        g_runtime_changed.notify_all();
        LOGGER_E("gtk.runtime.start: std::thread creation failed error=%s", error.what());
        return false;
    }

    g_runtime_changed.wait(lock, [] { return g_state != GtkRuntimeState::starting; });
    const bool running = g_state == GtkRuntimeState::running;
    LOGGER_I("gtk.runtime.start: completed running=%d state=%s", running ? 1 : 0, state_name(g_state));
    lock.unlock();

    if (!running && g_gtk_thread.joinable()) {
        LOGGER_V("gtk.runtime.start: joining failed initialization thread");
        g_gtk_thread.join();
    }
    return running;
}

bool gtk_run_on_thread_async(std::function<void()> fn) {
    LOGGER_V("gtk.invoke.async.request: has_fn=%d", fn ? 1 : 0);
    if (!fn) {
        LOGGER_W("gtk.invoke.async.request: empty callback; skipping");
        return false;
    }
    if (gtk_is_gtk_thread()) {
        LOGGER_V("gtk.invoke.async.request: already on GTK thread; executing inline");
        fn();
        return true;
    }

    auto* call = new AsyncCall{std::move(fn)};
    std::lock_guard<std::mutex> lock(g_runtime_mutex);
    if (!attach_idle_source_locked(run_async_call, call, destroy_async_call)) {
        LOGGER_W("gtk.invoke.async.request: runtime rejected call=%p", call);
        delete call;
        return false;
    }
    return true;
}

bool gtk_run_on_thread_sync(const std::function<void()>& fn) {
    LOGGER_V("gtk.invoke.sync.request: has_fn=%d", fn ? 1 : 0);
    if (!fn) {
        LOGGER_W("gtk.invoke.sync.request: empty callback; skipping");
        return false;
    }
    if (gtk_is_gtk_thread()) {
        LOGGER_V("gtk.invoke.sync.request: already on GTK thread; executing inline");
        fn();
        return true;
    }

    auto call = std::make_shared<SyncCall>();
    call->fn = fn;
    {
        std::lock_guard<std::mutex> lock(g_runtime_mutex);
        // The source owns one shared_ptr until it is dispatched or destroyed.
        auto* source_ref = new std::shared_ptr<SyncCall>(call);
        auto dispatch = [](gpointer data) -> gboolean {
            auto* ref = static_cast<std::shared_ptr<SyncCall>*>(data);
            return run_sync_call(ref->get());
        };
        auto release = [](gpointer data) {
            delete static_cast<std::shared_ptr<SyncCall>*>(data);
        };
        if (!attach_idle_source_locked(dispatch, source_ref, release)) {
            delete source_ref;
            LOGGER_W("gtk.invoke.sync.request: runtime rejected call=%p", call.get());
            return false;
        }
    }

    LOGGER_V("gtk.invoke.sync.wait: call=%p", call.get());
    std::unique_lock<std::mutex> call_lock(call->mutex);
    call->changed.wait(call_lock, [&] { return call->done; });
    LOGGER_V("gtk.invoke.sync.wait: completed call=%p exception=%d",
             call.get(), call->exception ? 1 : 0);
    if (call->exception) std::rethrow_exception(call->exception);
    return true;
}

void gtk_stop() {
    LOGGER_I("gtk.runtime.stop: requested caller_is_gtk=%d", gtk_is_gtk_thread() ? 1 : 0);
    std::thread thread_to_join;
    {
        std::unique_lock<std::mutex> lock(g_runtime_mutex);
        while (g_state == GtkRuntimeState::starting) {
            LOGGER_V("gtk.runtime.stop: waiting for startup");
            g_runtime_changed.wait(lock);
        }
        if (g_state == GtkRuntimeState::stopped) {
            LOGGER_D("gtk.runtime.stop: already stopped joinable=%d", g_gtk_thread.joinable() ? 1 : 0);
            if (g_gtk_thread.joinable() && std::this_thread::get_id() != g_gtk_thread.get_id()) {
                thread_to_join = std::move(g_gtk_thread);
            }
        } else if (g_state == GtkRuntimeState::stopping) {
            LOGGER_D("gtk.runtime.stop: another caller is stopping runtime");
            if (std::this_thread::get_id() == g_gtk_thread_id) return;
            g_runtime_changed.wait(lock, [] { return g_state == GtkRuntimeState::stopped; });
            if (g_gtk_thread.joinable()) thread_to_join = std::move(g_gtk_thread);
        } else {
            g_state = GtkRuntimeState::stopping;
            LOGGER_D("gtk.runtime.stop: scheduling quit loop=%p context=%p", g_loop, g_context);
            GSource* source = g_idle_source_new();
            if (!source) {
                LOGGER_E("gtk.runtime.stop: unable to allocate quit source; forcing g_main_loop_quit");
                g_main_loop_quit(g_loop);
            } else {
                // Every invocation accepted while state==running was attached
                // before this source at the same priority. GLib dispatches
                // equal-priority sources in attachment order, so all accepted
                // synchronous waiters complete before the loop quits.
                g_source_set_priority(source, G_PRIORITY_DEFAULT);
                g_source_set_callback(source, quit_loop, g_loop, nullptr);
                const guint id = g_source_attach(source, g_context);
                g_source_unref(source);
                LOGGER_V("gtk.runtime.stop: quit source attached id=%u", id);
                g_main_context_wakeup(g_context);
            }

            if (std::this_thread::get_id() == g_gtk_thread_id) {
                LOGGER_W("gtk.runtime.stop: called on GTK thread; quit scheduled without self-join");
                return;
            }
            g_runtime_changed.wait(lock, [] { return g_state == GtkRuntimeState::stopped; });
            if (g_gtk_thread.joinable()) thread_to_join = std::move(g_gtk_thread);
        }
    }

    if (thread_to_join.joinable()) {
        LOGGER_D("gtk.runtime.stop: joining GTK thread");
        thread_to_join.join();
        LOGGER_I("gtk.runtime.stop: GTK thread joined");
    }
}

} // namespace wvbridge

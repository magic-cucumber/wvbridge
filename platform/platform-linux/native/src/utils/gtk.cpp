#include "gtk.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <atomic>
#include <future>
#include <thread>

#include <wvbridge/logger.h>

namespace wvbridge {

    static std::thread g_gtk_thread;
    static std::thread::id g_gtk_thread_id;
    static std::atomic_bool g_started{false};
    static std::atomic_bool g_stopping{false};

    static GMainLoop* g_loop = nullptr;
    static std::shared_future<void> g_ready_future;

    bool gtk_is_gtk_thread() {
        bool result = std::this_thread::get_id() == g_gtk_thread_id;
        LOGGER_V("gtk_is_gtk_thread: result=%d", result ? 1 : 0);
        return result;
    }

    bool gtk_is_inited() {
        LOGGER_I("gtk_is_inited:");
        if (!g_started.load(std::memory_order_acquire)) {
            LOGGER_V("gtk_is_inited: not started");
            return false;
        }

        // gtk_init() 会等待 g_ready_future；但为了允许外部在并发场景下探测，
        // 这里仍然做一次非阻塞检查。
        if (g_ready_future.valid() &&
            g_ready_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            LOGGER_V("gtk_is_inited: future not ready");
            return false;
        }

        bool result = g_loop != nullptr;
        LOGGER_V("gtk_is_inited: result=%d", result ? 1 : 0);
        return result;
    }

    void gtk_init() {
        LOGGER_I("gtk_init:");
        if (g_started.load(std::memory_order_acquire)) {
            LOGGER_W("gtk_init: already started, aborting");
            return;
        }

        // 按需求：不做额外线程安全保护（由 JVM 控制调用顺序）。
        g_started.store(true, std::memory_order_release);

        std::promise<void> ready_promise;
        g_ready_future = ready_promise.get_future().share();

        LOGGER_V("gtk_init: launching GTK thread");
        g_gtk_thread = std::thread([p = std::move(ready_promise)]() mutable {
            g_gtk_thread_id = std::this_thread::get_id();

            // GTK 初始化 + 默认主循环
            ::gtk_init(nullptr, nullptr);

            g_loop = g_main_loop_new(nullptr, FALSE);

            p.set_value();

            g_main_loop_run(g_loop);

            if (g_loop) {
                g_main_loop_unref(g_loop);
                g_loop = nullptr;
            }
        });

        g_ready_future.wait();
        LOGGER_V("gtk_init: GTK thread ready");
    }

    struct InvokeData {
        std::function<void()> fn;
        std::promise<void>* done = nullptr; // 同步调用时使用
    };

    static gboolean invoke_cb(gpointer data) {
        auto* d = static_cast<InvokeData*>(data);
        LOGGER_V("invoke_cb: data=%p, has_fn=%d, has_done=%d", data, d->fn ? 1 : 0, d->done ? 1 : 0);

        try {
            if (d->fn) d->fn();
        } catch (...) {
            LOGGER_W("invoke_cb: exception caught in callback");
            // 这里吞掉异常：JNI 边界如何处理异常由上层决定
        }

        if (d->done) {
            d->done->set_value();
        }

        delete d;
        return G_SOURCE_REMOVE;
    }

    void gtk_run_on_thread_async(std::function<void()> fn) {
        LOGGER_I("gtk_run_on_thread_async:");

        if (gtk_is_gtk_thread()) {
            LOGGER_V("gtk_run_on_thread_async: already on GTK thread, executing directly");
            if (fn) fn();
            return;
        }

        LOGGER_V("gtk_run_on_thread_async: invoking on GTK main context");
        auto* d = new InvokeData{std::move(fn), nullptr};
        g_main_context_invoke(nullptr, invoke_cb, d);
    }

    void gtk_run_on_thread_sync(const std::function<void()>& fn) {
        LOGGER_I("gtk_run_on_thread_sync:");

        if (gtk_is_gtk_thread()) {
            LOGGER_V("gtk_run_on_thread_sync: already on GTK thread, executing directly");
            if (fn) fn();
            return;
        }

        LOGGER_V("gtk_run_on_thread_sync: invoking on GTK main context (sync)");
        std::promise<void> done;
        auto fut = done.get_future();

        auto* d = new InvokeData{fn, &done};
        g_main_context_invoke(nullptr, invoke_cb, d);

        fut.wait();
        LOGGER_V("gtk_run_on_thread_sync: done waiting");
    }

    void gtk_stop() {
        LOGGER_I("gtk_stop:");
        if (!g_started.load(std::memory_order_acquire)) {
            LOGGER_W("gtk_stop: not started, aborting");
            return;
        }
        if (g_stopping.exchange(true)) {
            LOGGER_W("gtk_stop: already stopping, aborting");
            return;
        }

        LOGGER_V("gtk_stop: quitting main loop");
        // 让主循环退出
        gtk_run_on_thread_async([] {
            if (g_loop) g_main_loop_quit(g_loop);
        });

        if (g_gtk_thread.joinable()) {
            LOGGER_V("gtk_stop: joining GTK thread");
            g_gtk_thread.join();
        }

        g_stopping.store(false);
        g_started.store(false, std::memory_order_release);
        g_gtk_thread_id = {};
    }

} // namespace wvbridge

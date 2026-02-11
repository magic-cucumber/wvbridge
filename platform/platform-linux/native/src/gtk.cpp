#include "gtk.h"

#include <gtk/gtk.h>
#include <glib.h>

#include <atomic>
#include <future>
#include <thread>

namespace wvbridge {

    static std::thread g_gtk_thread;
    static std::thread::id g_gtk_thread_id;
    static std::atomic_bool g_started{false};
    static std::atomic_bool g_stopping{false};

    static GMainLoop* g_loop = nullptr;
    static std::shared_future<void> g_ready_future;

    bool gtk_is_gtk_thread() {
        return std::this_thread::get_id() == g_gtk_thread_id;
    }

    bool gtk_is_inited() {
        if (!g_started.load(std::memory_order_acquire)) return false;

        // gtk_init() 会等待 g_ready_future；但为了允许外部在并发场景下探测，
        // 这里仍然做一次非阻塞检查。
        if (g_ready_future.valid() &&
            g_ready_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return false;
        }

        return g_loop != nullptr;
    }

    void gtk_init() {
        if (g_started.load(std::memory_order_acquire)) return;

        // 按需求：不做额外线程安全保护（由 JVM 控制调用顺序）。
        g_started.store(true, std::memory_order_release);

        std::promise<void> ready_promise;
        g_ready_future = ready_promise.get_future().share();

        g_gtk_thread = std::thread([p = std::move(ready_promise)]() mutable {
            g_gtk_thread_id = std::this_thread::get_id();

            // GTK 初始化 + 默认主循环
            ::gtk_init();

            g_loop = g_main_loop_new(nullptr, FALSE);

            p.set_value();

            g_main_loop_run(g_loop);

            if (g_loop) {
                g_main_loop_unref(g_loop);
                g_loop = nullptr;
            }
        });

        g_ready_future.wait();
    }

    struct InvokeData {
        std::function<void()> fn;
        std::promise<void>* done = nullptr; // 同步调用时使用
    };

    static gboolean invoke_cb(gpointer data) {
        auto* d = static_cast<InvokeData*>(data);

        try {
            if (d->fn) d->fn();
        } catch (...) {
            // 这里吞掉异常：JNI 边界如何处理异常由上层决定
        }

        if (d->done) {
            d->done->set_value();
        }

        delete d;
        return G_SOURCE_REMOVE;
    }

    void gtk_run_on_thread_async(std::function<void()> fn) {

        if (gtk_is_gtk_thread()) {
            if (fn) fn();
            return;
        }

        auto* d = new InvokeData{std::move(fn), nullptr};
        g_main_context_invoke(nullptr, invoke_cb, d);
    }

    void gtk_run_on_thread_sync(const std::function<void()>& fn) {
        if (gtk_is_gtk_thread()) {
            if (fn) fn();
            return;
        }

        std::promise<void> done;
        auto fut = done.get_future();

        auto* d = new InvokeData{fn, &done};
        g_main_context_invoke(nullptr, invoke_cb, d);

        fut.wait();
    }

    void gtk_stop() {
        if (!g_started.load(std::memory_order_acquire)) return;
        if (g_stopping.exchange(true)) return;

        // 让主循环退出
        gtk_run_on_thread_async([] {
            if (g_loop) g_main_loop_quit(g_loop);
        });

        if (g_gtk_thread.joinable()) g_gtk_thread.join();

        g_stopping.store(false);
        g_started.store(false, std::memory_order_release);
        g_gtk_thread_id = {};
    }

} // namespace wvbridge

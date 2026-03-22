#include "thread.h"

#include <windows.h>
#include <objbase.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

namespace {
constexpr UINT WM_RUN_TASKS = WM_APP + 0x4D2;
}

struct WebView2Thread {
    std::thread worker;
    DWORD thread_id = 0;

    std::mutex queue_mutex;
    std::queue<std::function<void()>> tasks;

    std::mutex init_mutex;
    std::condition_variable init_cv;
    bool ready = false;
};

static void thread_entry(WebView2Thread *thread) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    MSG msg{};
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    {
        std::lock_guard<std::mutex> lk(thread->init_mutex);
        thread->thread_id = GetCurrentThreadId();
        thread->ready = true;
    }
    thread->init_cv.notify_one();

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_RUN_TASKS) {
            while (true) {
                std::function<void()> task;
                {
                    std::lock_guard<std::mutex> lk(thread->queue_mutex);
                    if (thread->tasks.empty()) break;
                    task = std::move(thread->tasks.front());
                    thread->tasks.pop();
                }
                if (task) task();
            }
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }
}

WebView2Thread *webview2_thread_create() {
    auto *thread = new WebView2Thread();
    thread->worker = std::thread(thread_entry, thread);

    std::unique_lock<std::mutex> lk(thread->init_mutex);
    thread->init_cv.wait(lk, [thread] { return thread->ready; });
    return thread;
}

void webview2_thread_destroy(WebView2Thread *thread) {
    if (!thread) return;

    if (thread->thread_id != 0) {
        PostThreadMessageW(thread->thread_id, WM_QUIT, 0, 0);
    }
    if (thread->worker.joinable()) {
        thread->worker.join();
    }
    delete thread;
}

void webview2_thread_run_async(WebView2Thread *thread, std::function<void()> task) {
    if (!thread || !task) return;

    {
        std::lock_guard<std::mutex> lk(thread->queue_mutex);
        thread->tasks.push(std::move(task));
    }
    PostThreadMessageW(thread->thread_id, WM_RUN_TASKS, 0, 0);
}

void webview2_thread_run_sync(WebView2Thread *thread, const std::function<void()> &task) {
    if (!thread || !task) return;
    if (GetCurrentThreadId() == thread->thread_id) {
        task();
        return;
    }

    auto completion = std::make_shared<std::promise<void>>();
    auto future = completion->get_future();

    webview2_thread_run_async(thread, [task, completion] {
        task();
        completion->set_value();
    });

    future.wait();
}

bool webview2_thread_is_current(WebView2Thread *thread) {
    return thread && GetCurrentThreadId() == thread->thread_id;
}

#include "thread.h"

#include <windows.h>
#include <objbase.h>

#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>

#include <wvbridge/logger.h>

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
    LOGGER_I("thread_entry: thread=%p", thread);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LOGGER_V("thread_entry: CoInitializeEx hr=0x%lx", hr);

    MSG msg{};
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    {
        std::lock_guard<std::mutex> lk(thread->init_mutex);
        thread->thread_id = GetCurrentThreadId();
        thread->ready = true;
    }
    thread->init_cv.notify_one();
    LOGGER_V("thread_entry: thread ready, tid=%lu", thread->thread_id);

    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_RUN_TASKS) {
            LOGGER_V("thread_entry: processing WM_RUN_TASKS");
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

    LOGGER_V("thread_entry: message loop exited");
    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }
}

WebView2Thread *webview2_thread_create() {
    LOGGER_I("webview2_thread_create");
    auto *thread = new WebView2Thread();
    thread->worker = std::thread(thread_entry, thread);
    LOGGER_V("webview2_thread_create: thread spawned, waiting for ready");

    std::unique_lock<std::mutex> lk(thread->init_mutex);
    thread->init_cv.wait(lk, [thread] { return thread->ready; });
    LOGGER_V("webview2_thread_create: thread ready, tid=%lu", thread->thread_id);
    return thread;
}

void webview2_thread_destroy(WebView2Thread *thread) {
    LOGGER_I("webview2_thread_destroy: thread=%p", thread);
    if (!thread) {
        LOGGER_W("webview2_thread_destroy: null thread, aborting");
        return;
    }

    if (thread->thread_id != 0) {
        LOGGER_V("webview2_thread_destroy: posting WM_QUIT to tid=%lu", thread->thread_id);
        PostThreadMessageW(thread->thread_id, WM_QUIT, 0, 0);
    }
    if (thread->worker.joinable()) {
        LOGGER_V("webview2_thread_destroy: joining worker thread");
        thread->worker.join();
    }
    delete thread;
    LOGGER_V("webview2_thread_destroy: thread destroyed");
}

void webview2_thread_run_async(WebView2Thread *thread, std::function<void()> task) {
    LOGGER_V("webview2_thread_run_async: thread=%p", thread);
    if (!thread || !task) {
        LOGGER_W("webview2_thread_run_async: null param, aborting");
        return;
    }

    {
        std::lock_guard<std::mutex> lk(thread->queue_mutex);
        thread->tasks.push(std::move(task));
    }
    LOGGER_V("webview2_thread_run_async: task queued, posting WM_RUN_TASKS");
    PostThreadMessageW(thread->thread_id, WM_RUN_TASKS, 0, 0);
}

void webview2_thread_run_sync(WebView2Thread *thread, const std::function<void()> &task) {
    LOGGER_V("webview2_thread_run_sync: thread=%p", thread);
    if (!thread || !task) {
        LOGGER_W("webview2_thread_run_sync: null param, aborting");
        return;
    }
    if (GetCurrentThreadId() == thread->thread_id) {
        LOGGER_V("webview2_thread_run_sync: already on target thread, executing directly");
        task();
        return;
    }

    auto completion = std::make_shared<std::promise<void>>();
    auto future = completion->get_future();

    LOGGER_V("webview2_thread_run_sync: dispatching async and waiting");
    webview2_thread_run_async(thread, [task, completion] {
        task();
        completion->set_value();
    });

    future.wait();
    LOGGER_V("webview2_thread_run_sync: task completed");
}

bool webview2_thread_is_current(WebView2Thread *thread) {
    LOGGER_V("webview2_thread_is_current: thread=%p", thread);
    return thread && GetCurrentThreadId() == thread->thread_id;
}

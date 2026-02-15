#include "thread.h"
#include <windows.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

// 自定义消息，用于通知线程处理任务队列
#define WM_RUN_TASKS (WM_APP + 1)

// 0. 定义 WebView2Environment 结构体 (内部字段私有)
struct WebView2Environment {
    std::thread workerThread;
    DWORD threadId = 0; // 目标线程的 Windows ID，用于 PostThreadMessage

    // 任务队列相关
    std::mutex queueMutex;
    std::queue<std::function<void()>> taskQueue;

    // 初始化同步相关
    std::mutex initMutex;
    std::condition_variable initCv;
    bool ready = false;
};

// 线程的主循环函数
static void ThreadEntryPoint(WebView2Environment* env) {
    // A. 初始化 COM 为 STA 模式 (WebView2 必须)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // B. 强制创建消息队列
    // 在调用 GetMessage 之前，Windows 可能还没为该线程分配消息队列。
    // 调用 PeekMessage 会强制系统为当前线程创建消息队列。
    MSG msg;
    PeekMessage(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // C. 获取线程 ID
    env->threadId = GetCurrentThreadId();

    // D. 通知主线程：环境已准备就绪
    {
        std::lock_guard<std::mutex> lock(env->initMutex);
        env->ready = true;
    }
    env->initCv.notify_one();

    // E. 启动消息循环 (Message Pump)
    // 这是 WebView2 和大多数 Windows UI 组件存活的关键
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_RUN_TASKS) {
            // 处理任务队列中的所有任务
            while (true) {
                std::function<void()> task;
                {
                    std::lock_guard<std::mutex> lock(env->queueMutex);
                    if (env->taskQueue.empty()) break;
                    task = std::move(env->taskQueue.front());
                    env->taskQueue.pop();
                }
                // 在锁外执行任务，防止死锁
                if (task) {
                    task();
                }
            }
        } else {
            // 标准消息分发
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // F. 清理 COM
    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }
}

WebView2Environment* webview2_environment_init() {
    auto* env = new WebView2Environment();

    // 启动线程
    env->workerThread = std::thread(ThreadEntryPoint, env);

    // 等待线程初始化完成 (确保 threadId 有效且消息队列已创建)
    {
        std::unique_lock<std::mutex> lock(env->initMutex);
        env->initCv.wait(lock, [env] { return env->ready; });
    }

    return env;
}

void webview2_environment_destroy(WebView2Environment* env) {
    if (!env) return;

    if (env->threadId != 0) {
        // 发送 WM_QUIT 消息退出 GetMessage 循环
        PostThreadMessage(env->threadId, WM_QUIT, 0, 0);
    }

    if (env->workerThread.joinable()) {
        env->workerThread.join();
    }

    delete env;
}

void webview2_environment_run_async(WebView2Environment* env, std::function<void()> task) {
    if (!env || !task) return;

    // 1. 将任务推入队列
    {
        std::lock_guard<std::mutex> lock(env->queueMutex);
        env->taskQueue.push(std::move(task));
    }

    // 2. 发送消息唤醒目标线程
    // 目标线程会在 GetMessage 中收到 WM_RUN_TASKS 并执行队列
    PostThreadMessage(env->threadId, WM_RUN_TASKS, 0, 0);
}

void webview2_environment_run_sync(WebView2Environment* env, std::function<void()> task) {
    if (!env || !task) return;

    // 如果当前就是目标线程，直接运行，避免自己等自己导致死锁
    if (GetCurrentThreadId() == env->threadId) {
        task();
        return;
    }

    // 使用 promise/future 实现同步等待
    auto promise = std::make_shared<std::promise<void>>();
    auto future = promise->get_future();

    // 包装用户的 task，在执行完后设置 promise
    auto wrapper = [task, promise]() {
        task();
        promise->set_value();
    };

    // 复用异步机制
    webview2_environment_run_async(env, wrapper);

    // 阻塞等待结果
    future.wait();
}

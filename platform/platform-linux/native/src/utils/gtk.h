#pragma once

#include <functional>

namespace wvbridge {

// 启动专用 GTK 线程并运行默认 GLib 主循环。
// 返回 false 表示 GTK 初始化失败或运行时已经进入停止阶段。
    bool gtk_init();

// GTK 是否已经初始化完成（主循环已创建并就绪）。
    bool gtk_is_inited();

// 停止 GLib 主循环并等待 GTK 线程退出。可重复调用。
    void gtk_stop();

// 在 GTK 线程同步执行闭包；若运行时正在停止则返回 false。
    bool gtk_run_on_thread_sync(const std::function<void()>& fn);

// 在 GTK 线程异步执行闭包；若运行时正在停止则返回 false。
    bool gtk_run_on_thread_async(std::function<void()> fn);

// 当前调用线程是否为 GTK 线程。
    bool gtk_is_gtk_thread();

} // namespace wvbridge

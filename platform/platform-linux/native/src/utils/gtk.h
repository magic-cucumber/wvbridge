#pragma once

#include <functional>

namespace wvbridge {

// 启动 GTK 主线程并运行 GLib 主循环（仅初始化一次）。
    void gtk_init();

// GTK 是否已经初始化完成（主循环已创建并就绪）。
    bool gtk_is_inited();

// 停止 GLib 主循环并等待 GTK 线程退出。
    void gtk_stop();

// 在 GTK 线程同步执行闭包；若当前已在 GTK 线程，直接执行。
    void gtk_run_on_thread_sync(const std::function<void()>& fn);

// 在 GTK 线程异步执行闭包；立即返回。
    void gtk_run_on_thread_async(std::function<void()> fn);

// 当前调用线程是否为 GTK 线程。
    bool gtk_is_gtk_thread();

} // namespace wvbridge

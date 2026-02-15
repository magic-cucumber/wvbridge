#pragma once

#include <functional>

// 1. 定义 opaque 指针，隐藏内部实现细节
struct WebView2Environment;

// 2. 初始化 COM 环境，启动带有消息循环的专用线程
WebView2Environment* webview2_environment_init();

// 3. 销毁线程并清理资源
void webview2_environment_destroy(WebView2Environment* env);

// 4. 异步执行：提交任务后立即返回
void webview2_environment_run_async(WebView2Environment* env, std::function<void()> task);

// 5. 同步执行：提交任务并阻塞当前线程，直到任务在目标线程执行完毕
void webview2_environment_run_sync(WebView2Environment* env, std::function<void()> task);

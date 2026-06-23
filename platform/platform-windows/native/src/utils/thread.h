#pragma once

#include <functional>

struct WebView2Thread;

WebView2Thread *webview2_thread_create();
void webview2_thread_destroy(WebView2Thread *thread);
void webview2_thread_run_async(WebView2Thread *thread, std::function<void()> task);
void webview2_thread_run_sync(WebView2Thread *thread, const std::function<void()> &task);
bool webview2_thread_is_current(WebView2Thread *thread);

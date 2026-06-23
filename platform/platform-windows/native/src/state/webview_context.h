#pragma once

#include <Unknwn.h>
#include <windows.h>

#include <WebView2.h>
#include <wrl.h>

#include <atomic>

#include "thread.h"

struct WebViewEvents;

struct WebViewContext {
    HWND parent_hwnd = nullptr;
    HWND child_hwnd = nullptr;

    WebView2Thread *thread = nullptr;
    std::atomic_bool closing{false};

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> env;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;
    WebViewEvents* events = nullptr;
};

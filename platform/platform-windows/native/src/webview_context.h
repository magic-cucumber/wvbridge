#pragma once

#include <Unknwn.h>
#include <windows.h>

#include <WebView2.h>
#include <wrl.h>

#include <atomic>
#include <jni.h>
#include <mutex>

#include "thread.h"

struct JavaListenerState {
    std::mutex mutex;
    jobject global = nullptr;
    jmethodID mid = nullptr;
    bool use_accept = true;
    bool two_args = false;
};

struct WebViewContext {
    JavaVM *jvm = nullptr;
    HWND parent_hwnd = nullptr;
    HWND child_hwnd = nullptr;

    WebView2Thread *thread = nullptr;
    std::atomic_bool closing{false};

    Microsoft::WRL::ComPtr<ICoreWebView2Environment> env;
    Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller;
    Microsoft::WRL::ComPtr<ICoreWebView2> webview;

    EventRegistrationToken token_source_changed{};
    EventRegistrationToken token_nav_starting{};
    EventRegistrationToken token_content_loading{};
    EventRegistrationToken token_nav_completed{};
    EventRegistrationToken token_history_changed{};
    EventRegistrationToken token_new_window_requested{};
    bool source_changed_registered = false;
    bool nav_starting_registered = false;
    bool content_loading_registered = false;
    bool nav_completed_registered = false;
    bool history_changed_registered = false;
    bool new_window_requested_registered = false;

    JavaListenerState url_listener;
    JavaListenerState page_loading_start_listener;
    JavaListenerState page_loading_progress_listener;
    JavaListenerState page_loading_end_listener;
    JavaListenerState can_go_back_change_listener;
    JavaListenerState can_go_forward_change_listener;
};

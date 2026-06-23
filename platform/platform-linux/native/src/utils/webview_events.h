#pragma once

#include <jni.h>

#include <atomic>

#include <webkit2/webkit2.h>

namespace wvbridge {

struct WebViewEvents;

WebViewEvents* webview_events_create(
    WebKitWebView* webview,
    jlong pointer,
    const std::atomic_bool* closing
);

void webview_events_destroy(WebViewEvents* events);

} // namespace wvbridge

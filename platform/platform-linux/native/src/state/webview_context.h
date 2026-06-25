#pragma once

#include <X11/Xlib.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include <gtk/gtk.h>
#include <jni.h>
#include <webkit2/webkit2.h>

#include <wvbridge/javascript.h>

namespace wvbridge {
struct WebViewEvents;
}

struct WebViewContext {
    ::Window parent_xid = 0;
    ::Window child_xid = 0;
    ::Display *xdisplay = nullptr;

    GtkWidget *window = nullptr;
    WebKitWebView *webview = nullptr;
    wvbridge::WebViewEvents* events = nullptr;

    std::atomic_bool closing{false};

    gulong window_button_press_handler_id = 0;
    gulong webview_button_press_handler_id = 0;
    gulong web_message_handler_id = 0;
    jlong next_document_start_hook_id = 1;
    std::map<jlong, WebKitUserScript *> document_start_hooks;
    jlong next_web_message_handler_id = 1;
    std::mutex web_message_handlers_mutex;
    wvbridge::WebMessageHandlers web_message_handlers;
};

#include "url_listener.h"

#include <glib.h>

#include <cstring>
#include <mutex>
#include <string>

#include "wvbridge/java_caller.h"
#include "wvbridge/java_runtime.h"

namespace wvbridge {

struct URLListenerState {
    std::mutex listener_mutex;
    java_caller* listener = nullptr;

    gulong notify_uri_handler_id = 0;

    const std::atomic_bool* closing = nullptr; // borrowed
};

static bool should_skip_due_to_closing(const URLListenerState* st) {
    if (!st) return false;
    if (!st->closing) return false;
    return st->closing->load(std::memory_order_acquire);
}

static java_caller* create_listener(JNIEnv* env, jobject listener) {
    if (!env || !listener) return nullptr;

    java_caller* caller = nullptr;
    java_caller_status status = java_caller_create(env, listener, "accept", "(Ljava/lang/Object;)V", &caller);
    if (status == JAVA_CALLER_ERR_METHOD_NOT_FOUND) {
        status = java_caller_create(env, listener, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;", &caller);
    }
    return status == JAVA_CALLER_OK ? caller : nullptr;
}

static void notify_listener(URLListenerState* st, const char* uri) {
    if (!st) return;

    java_caller* caller = nullptr;
    {
        std::lock_guard<std::mutex> lock(st->listener_mutex);
        caller = java_caller_retain(st->listener);
    }
    if (!caller) return;

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (!env) {
        java_caller_release(caller);
        return;
    }

    jstring juri = env->NewStringUTF(uri ? uri : "");
    if (juri) {
        jvalue args[1];
        std::memset(args, 0, sizeof(args));
        args[0].l = juri;
        java_caller_invoke(caller, args, nullptr);
        env->DeleteLocalRef(juri);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

static void notify_uri_cb(WebKitWebView* webview, GParamSpec* pspec, gpointer user_data) {
    auto* st = static_cast<URLListenerState*>(user_data);
    if (!st || !webview) return;
    if (pspec == nullptr) return;
    if (should_skip_due_to_closing(st)) return;

    const char* uri = webkit_web_view_get_uri(webview);
    notify_listener(st, uri ? uri : "");
}

URLListenerState* url_listener_state_new(JNIEnv* env) {
    (void) env;
    return new URLListenerState();
}

void url_listener_state_destroy(URLListenerState* state) {
    if (!state) return;

    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = state->listener;
        state->listener = nullptr;
    }
    java_caller_destroy(old);
    delete state;
}

void url_listener_set_listener(JNIEnv* env, URLListenerState* state, jobject listener) {
    if (!state || !env) return;

    java_caller* caller = create_listener(env, listener);
    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = state->listener;
        state->listener = caller;
    }
    java_caller_destroy(old);
}

void url_listener_install(WebKitWebView* webview, URLListenerState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }

    state->notify_uri_handler_id = g_signal_connect(webview, "notify::uri", G_CALLBACK(notify_uri_cb), state);
}

void url_listener_uninstall(WebKitWebView* webview, URLListenerState* state) {
    if (!webview || !state) return;

    if (state->notify_uri_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->notify_uri_handler_id);
        state->notify_uri_handler_id = 0;
    }
}

} // namespace wvbridge

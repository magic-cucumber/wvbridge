#include "page_loading.h"

#include <glib.h>

#include <algorithm>
#include <string>

#include "java_listener.h"

namespace wvbridge {

struct PageLoadingState {
    JavaListener* start_listener = nullptr;
    JavaListener* progress_listener = nullptr;
    JavaListener* end_listener = nullptr;

    gulong load_changed_handler_id = 0;
    gulong progress_handler_id = 0;
    gulong load_failed_handler_id = 0;

    const std::atomic_bool* closing = nullptr;
    bool last_load_failed = false;
    std::string last_error_reason;
};

namespace {

bool should_skip_due_to_closing(const PageLoadingState* state) {
    if (!state || !state->closing) return false;
    return state->closing->load(std::memory_order_acquire);
}

float clamp01(double value) {
    if (value != value) return 0.0f;
    if (value < 0.0) return 0.0f;
    if (value > 1.0) return 1.0f;
    return static_cast<float>(value);
}

void notify_load_changed_cb(WebKitWebView* webview, WebKitLoadEvent load_event, gpointer user_data) {
    auto* state = static_cast<PageLoadingState*>(user_data);
    if (!state || !webview || should_skip_due_to_closing(state)) return;

    switch (load_event) {
        case WEBKIT_LOAD_STARTED: {
            state->last_load_failed = false;
            state->last_error_reason.clear();
            java_listener_notify_string(state->start_listener, webkit_web_view_get_uri(webview));
            java_listener_notify_float(state->progress_listener, 0.0f);
            break;
        }
        case WEBKIT_LOAD_COMMITTED: {
            const float progress = clamp01(webkit_web_view_get_estimated_load_progress(webview));
            java_listener_notify_float(state->progress_listener, std::max(progress, 0.1f));
            break;
        }
        case WEBKIT_LOAD_FINISHED: {
            java_listener_notify_float(state->progress_listener, 1.0f);
            const bool success = !state->last_load_failed;
            java_listener_notify_boolean_string(
                state->end_listener,
                success,
                success ? nullptr : state->last_error_reason.c_str()
            );
            break;
        }
        case WEBKIT_LOAD_REDIRECTED:
            break;
    }
}

gboolean notify_progress_cb(GObject* object, GParamSpec* pspec, gpointer user_data) {
    (void) pspec;

    auto* state = static_cast<PageLoadingState*>(user_data);
    if (!state || should_skip_due_to_closing(state)) return FALSE;

    auto* webview = WEBKIT_WEB_VIEW(object);
    java_listener_notify_float(
        state->progress_listener,
        clamp01(webkit_web_view_get_estimated_load_progress(webview))
    );
    return FALSE;
}

gboolean load_failed_cb(WebKitWebView* webview,
                        WebKitLoadEvent load_event,
                        const gchar* failing_uri,
                        GError* error,
                        gpointer user_data) {
    (void) webview;
    (void) load_event;
    (void) failing_uri;
    (void) error;

    auto* state = static_cast<PageLoadingState*>(user_data);
    if (!state || should_skip_due_to_closing(state)) return FALSE;

    state->last_load_failed = true;
    std::string reason = "webkitgtk.load-failed";
    if (error) {
        reason += ": domain=";
        const char* domain_name = g_quark_to_string(error->domain);
        reason += domain_name ? domain_name : "unknown";
        reason += ", code=" + std::to_string(error->code);
        if (error->message && error->message[0] != '\0') {
            reason += ", message=";
            reason += error->message;
        }
    }
    if (failing_uri && failing_uri[0] != '\0') {
        reason += ", uri=";
        reason += failing_uri;
    }
    state->last_error_reason = reason;
    return FALSE;
}

} // namespace

PageLoadingState* page_loading_state_new(JNIEnv* env) {
    auto* state = new PageLoadingState();
    state->start_listener = java_listener_new(env);
    state->progress_listener = java_listener_new(env);
    state->end_listener = java_listener_new(env);
    if (!state->start_listener || !state->progress_listener || !state->end_listener) {
        java_listener_destroy(state->start_listener);
        java_listener_destroy(state->progress_listener);
        java_listener_destroy(state->end_listener);
        delete state;
        return nullptr;
    }
    return state;
}

void page_loading_state_destroy(PageLoadingState* state) {
    if (!state) return;
    java_listener_destroy(state->start_listener);
    java_listener_destroy(state->progress_listener);
    java_listener_destroy(state->end_listener);
    delete state;
}

void page_loading_set_start_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    if (!state) return;
    java_listener_set(env, state->start_listener, listener);
}

void page_loading_set_progress_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    if (!state) return;
    java_listener_set(env, state->progress_listener, listener);
}

void page_loading_set_end_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    if (!state) return;
    java_listener_set_two_args(env, state->end_listener, listener);
}

void page_loading_install(WebKitWebView* webview, PageLoadingState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;
    page_loading_uninstall(webview, state);

    state->load_changed_handler_id = g_signal_connect(webview, "load-changed", G_CALLBACK(notify_load_changed_cb), state);
    state->progress_handler_id = g_signal_connect(webview, "notify::estimated-load-progress", G_CALLBACK(notify_progress_cb), state);
    state->load_failed_handler_id = g_signal_connect(webview, "load-failed", G_CALLBACK(load_failed_cb), state);
}

void page_loading_uninstall(WebKitWebView* webview, PageLoadingState* state) {
    if (!webview || !state) return;

    if (state->load_changed_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->load_changed_handler_id);
        state->load_changed_handler_id = 0;
    }
    if (state->progress_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->progress_handler_id);
        state->progress_handler_id = 0;
    }
    if (state->load_failed_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->load_failed_handler_id);
        state->load_failed_handler_id = 0;
    }
}

} // namespace wvbridge

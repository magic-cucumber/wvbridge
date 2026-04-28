#include "page_loading.h"

#include <glib.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <string>

#include "wvbridge/java_caller.h"
#include "wvbridge/java_runtime.h"
#include "wvbridge/utils.h"

namespace wvbridge {

struct PageLoadingState {
    std::mutex listener_mutex;
    java_caller* start_listener = nullptr;
    java_caller* progress_listener = nullptr;
    java_caller* end_listener = nullptr;

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

java_caller* create_listener(JNIEnv* env, jobject listener, bool two_args) {
    if (!env || !listener) return nullptr;

    const char* accept_signature = two_args
        ? "(Ljava/lang/Object;Ljava/lang/Object;)V"
        : "(Ljava/lang/Object;)V";
    const char* invoke_signature = two_args
        ? "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
        : "(Ljava/lang/Object;)Ljava/lang/Object;";

    java_caller* caller = nullptr;
    java_caller_status status = java_caller_create(env, listener, "accept", accept_signature, &caller);
    if (status == JAVA_CALLER_ERR_METHOD_NOT_FOUND) {
        status = java_caller_create(env, listener, "invoke", invoke_signature, &caller);
    }
    return status == JAVA_CALLER_OK ? caller : nullptr;
}

void replace_listener(PageLoadingState* state, java_caller** slot, JNIEnv* env, jobject listener, bool two_args) {
    if (!state || !slot || !env) return;

    java_caller* caller = create_listener(env, listener, two_args);
    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = *slot;
        *slot = caller;
    }
    java_caller_destroy(old);
}

java_caller* retain_listener(PageLoadingState* state, java_caller* slot) {
    if (!state) return nullptr;

    std::lock_guard<std::mutex> lock(state->listener_mutex);
    return java_caller_retain(slot);
}

void destroy_listener(PageLoadingState* state, java_caller** slot) {
    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = *slot;
        *slot = nullptr;
    }
    java_caller_destroy(old);
}

void notify_string(PageLoadingState* state, java_caller* slot, const char* value) {
    java_caller* caller = retain_listener(state, slot);
    if (!caller) return;

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (!env) {
        java_caller_release(caller);
        return;
    }

    jstring boxed = env->NewStringUTF(value ? value : "");
    if (boxed) {
        jvalue args[1];
        std::memset(args, 0, sizeof(args));
        args[0].l = boxed;
        java_caller_invoke(caller, args, nullptr);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

void notify_float(PageLoadingState* state, java_caller* slot, float value) {
    java_caller* caller = retain_listener(state, slot);
    if (!caller) return;

    jvalue boxed;
    if (java_caller_pack_float(value, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        std::memset(args, 0, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

void notify_boolean_string(PageLoadingState* state, java_caller* slot, bool value, const char* message) {
    java_caller* caller = retain_listener(state, slot);
    if (!caller) return;

    jvalue boxed_bool;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxed_bool) != JAVA_CALLER_OK) {
        java_caller_release(caller);
        return;
    }

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (!env) {
        java_caller_delete_global_ref(boxed_bool.l);
        java_caller_release(caller);
        return;
    }

    jstring boxed_message = nullptr;
    if (message) {
        boxed_message = env->NewStringUTF(message);
        if (!boxed_message) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            java_caller_delete_global_ref(boxed_bool.l);
            java_runtime_detach_env(attached);
            java_caller_release(caller);
            return;
        }
    }

    jvalue args[2];
    std::memset(args, 0, sizeof(args));
    args[0] = boxed_bool;
    args[1].l = boxed_message;
    java_caller_invoke(caller, args, nullptr);

    if (boxed_message) env->DeleteLocalRef(boxed_message);
    java_caller_delete_global_ref(boxed_bool.l);
    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

void notify_load_changed_cb(WebKitWebView* webview, WebKitLoadEvent load_event, gpointer user_data) {
    auto* state = static_cast<PageLoadingState*>(user_data);
    if (!state || !webview || should_skip_due_to_closing(state)) return;

    switch (load_event) {
        case WEBKIT_LOAD_STARTED: {
            state->last_load_failed = false;
            state->last_error_reason.clear();
            notify_string(state, state->start_listener, webkit_web_view_get_uri(webview));
            notify_float(state, state->progress_listener, 0.0f);
            break;
        }
        case WEBKIT_LOAD_COMMITTED: {
            const float progress = clamp01(webkit_web_view_get_estimated_load_progress(webview));
            notify_float(state, state->progress_listener, std::max(progress, 0.1f));
            break;
        }
        case WEBKIT_LOAD_FINISHED: {
            notify_float(state, state->progress_listener, 1.0f);
            const bool success = !state->last_load_failed;
            notify_boolean_string(
                state,
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
    notify_float(
        state,
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
    (void) env;
    return new PageLoadingState();
}

void page_loading_state_destroy(PageLoadingState* state) {
    if (!state) return;
    destroy_listener(state, &state->start_listener);
    destroy_listener(state, &state->progress_listener);
    destroy_listener(state, &state->end_listener);
    delete state;
}

void page_loading_set_start_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    replace_listener(state, &state->start_listener, env, listener, false);
}

void page_loading_set_progress_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    replace_listener(state, &state->progress_listener, env, listener, false);
}

void page_loading_set_end_listener(JNIEnv* env, PageLoadingState* state, jobject listener) {
    replace_listener(state, &state->end_listener, env, listener, true);
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

#include "history_listener.h"

#include <glib.h>

#include <cstring>
#include <mutex>

#include "wvbridge/java_caller.h"
#include "wvbridge/utils.h"

namespace wvbridge {

struct HistoryListenerState {
    std::mutex listener_mutex;
    java_caller* can_go_back_listener = nullptr;
    java_caller* can_go_forward_listener = nullptr;

    gulong back_forward_list_changed_handler_id = 0;
    WebKitBackForwardList* back_forward_list = nullptr;
    WebKitWebView* webview = nullptr;

    const std::atomic_bool* closing = nullptr;
};

namespace {

bool should_skip_due_to_closing(const HistoryListenerState* state) {
    if (!state || !state->closing) return false;
    return state->closing->load(std::memory_order_acquire);
}

java_caller* create_listener(JNIEnv* env, jobject listener) {
    if (!env || !listener) return nullptr;

    java_caller* caller = nullptr;
    java_caller_status status = java_caller_create(env, listener, "accept", "(Ljava/lang/Object;)V", &caller);
    if (status == JAVA_CALLER_ERR_METHOD_NOT_FOUND) {
        status = java_caller_create(env, listener, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;", &caller);
    }
    return status == JAVA_CALLER_OK ? caller : nullptr;
}

void replace_listener(HistoryListenerState* state, java_caller** slot, JNIEnv* env, jobject listener) {
    if (!state || !slot || !env) return;

    java_caller* caller = create_listener(env, listener);
    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = *slot;
        *slot = caller;
    }
    java_caller_destroy(old);
}

void destroy_listener(HistoryListenerState* state, java_caller** slot) {
    java_caller* old = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        old = *slot;
        *slot = nullptr;
    }
    java_caller_destroy(old);
}

void notify_boolean(HistoryListenerState* state, java_caller* slot, bool value) {
    if (!state) return;

    java_caller* caller = nullptr;
    {
        std::lock_guard<std::mutex> lock(state->listener_mutex);
        caller = java_caller_retain(slot);
    }
    if (!caller) return;

    jvalue boxed;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        std::memset(args, 0, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

void notify_current(WebKitWebView* webview, HistoryListenerState* state) {
    if (!webview || !state || should_skip_due_to_closing(state)) return;

    notify_boolean(state, state->can_go_back_listener, webkit_web_view_can_go_back(webview));
    notify_boolean(state, state->can_go_forward_listener, webkit_web_view_can_go_forward(webview));
}

void back_forward_list_changed_cb(WebKitBackForwardList* back_forward_list,
                                  WebKitBackForwardListItem* added_item,
                                  GList* removed_items,
                                  gpointer user_data) {
    (void) back_forward_list;
    (void) added_item;
    (void) removed_items;

    auto* state = static_cast<HistoryListenerState*>(user_data);
    if (!state || should_skip_due_to_closing(state)) return;

    if (state->webview) {
        notify_current(state->webview, state);
    }
}

} // namespace

HistoryListenerState* history_listener_state_new(JNIEnv* env) {
    (void) env;
    return new HistoryListenerState();
}

void history_listener_state_destroy(HistoryListenerState* state) {
    if (!state) return;
    destroy_listener(state, &state->can_go_back_listener);
    destroy_listener(state, &state->can_go_forward_listener);
    delete state;
}

void history_listener_set_can_go_back_listener(JNIEnv* env, HistoryListenerState* state, jobject listener) {
    replace_listener(state, &state->can_go_back_listener, env, listener);
}

void history_listener_set_can_go_forward_listener(JNIEnv* env, HistoryListenerState* state, jobject listener) {
    replace_listener(state, &state->can_go_forward_listener, env, listener);
}

void history_listener_install(WebKitWebView* webview, HistoryListenerState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;
    history_listener_uninstall(webview, state);

    state->webview = webview;
    state->back_forward_list = webkit_web_view_get_back_forward_list(webview);
    if (!state->back_forward_list) return;

    state->back_forward_list_changed_handler_id = g_signal_connect(
        state->back_forward_list,
        "changed",
        G_CALLBACK(back_forward_list_changed_cb),
        state
    );

    notify_current(webview, state);
}

void history_listener_uninstall(WebKitWebView* webview, HistoryListenerState* state) {
    (void) webview;

    if (!state) return;
    if (state->back_forward_list && state->back_forward_list_changed_handler_id != 0) {
        g_signal_handler_disconnect(state->back_forward_list, state->back_forward_list_changed_handler_id);
        state->back_forward_list_changed_handler_id = 0;
    }
    state->back_forward_list = nullptr;
    state->webview = nullptr;
}

void history_listener_emit_current(WebKitWebView* webview, HistoryListenerState* state) {
    notify_current(webview, state);
}

} // namespace wvbridge

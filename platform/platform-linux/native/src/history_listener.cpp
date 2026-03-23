#include "history_listener.h"

#include <glib.h>

#include "java_listener.h"

namespace wvbridge {

struct HistoryListenerState {
    JavaListener* can_go_back_listener = nullptr;
    JavaListener* can_go_forward_listener = nullptr;

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

void notify_current(WebKitWebView* webview, HistoryListenerState* state) {
    if (!webview || !state || should_skip_due_to_closing(state)) return;

    java_listener_notify_boolean(state->can_go_back_listener, webkit_web_view_can_go_back(webview));
    java_listener_notify_boolean(state->can_go_forward_listener, webkit_web_view_can_go_forward(webview));
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
    auto* state = new HistoryListenerState();
    state->can_go_back_listener = java_listener_new(env);
    state->can_go_forward_listener = java_listener_new(env);
    if (!state->can_go_back_listener || !state->can_go_forward_listener) {
        java_listener_destroy(state->can_go_back_listener);
        java_listener_destroy(state->can_go_forward_listener);
        delete state;
        return nullptr;
    }
    return state;
}

void history_listener_state_destroy(HistoryListenerState* state) {
    if (!state) return;
    java_listener_destroy(state->can_go_back_listener);
    java_listener_destroy(state->can_go_forward_listener);
    delete state;
}

void history_listener_set_can_go_back_listener(JNIEnv* env, HistoryListenerState* state, jobject listener) {
    if (!state) return;
    java_listener_set(env, state->can_go_back_listener, listener);
}

void history_listener_set_can_go_forward_listener(JNIEnv* env, HistoryListenerState* state, jobject listener) {
    if (!state) return;
    java_listener_set(env, state->can_go_forward_listener, listener);
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

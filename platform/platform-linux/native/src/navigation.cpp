#include "navigation.h"

#include <glib.h>

namespace wvbridge {

struct NavigationState {
    gulong decide_policy_handler_id = 0;
    const std::atomic_bool* closing = nullptr; // borrowed
};

static bool should_block_due_to_closing(const NavigationState* st) {
    if (!st) return false;
    if (!st->closing) return false;
    return st->closing->load(std::memory_order_acquire);
}

static gboolean decide_policy_cb(WebKitWebView* webview,
                                 WebKitPolicyDecision* decision,
                                 WebKitPolicyDecisionType type,
                                 gpointer user_data) {
    auto* st = static_cast<NavigationState*>(user_data);
    if (!st || !webview || !decision) return FALSE;

    if (should_block_due_to_closing(st)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        auto* nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(nav_decision);
        if (action) {
            WebKitURIRequest* req = webkit_navigation_action_get_request(action);
            if (req) {
                // 将 window.open 等新窗口请求附加到当前窗口内加载。
                webkit_web_view_load_request(webview, req);
            }
        }
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        webkit_policy_decision_use(decision);
        return TRUE;
    }

    return FALSE;
}

NavigationState* navigation_state_new() {
    return new NavigationState();
}

void navigation_state_destroy(NavigationState* state) {
    if (!state) return;
    delete state;
}

void navigation_install(WebKitWebView* webview, NavigationState* state, const std::atomic_bool* closing_flag) {
    if (!webview || !state) return;

    state->closing = closing_flag;

    if (state->decide_policy_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->decide_policy_handler_id);
        state->decide_policy_handler_id = 0;
    }

    state->decide_policy_handler_id = g_signal_connect(webview, "decide-policy", G_CALLBACK(decide_policy_cb), state);
}

void navigation_uninstall(WebKitWebView* webview, NavigationState* state) {
    if (!webview || !state) return;

    if (state->decide_policy_handler_id != 0) {
        g_signal_handler_disconnect(webview, state->decide_policy_handler_id);
        state->decide_policy_handler_id = 0;
    }
}

} // namespace wvbridge

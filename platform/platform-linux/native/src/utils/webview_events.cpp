#include "webview_events.h"

#include <glib.h>

#include <algorithm>
#include <string>
#include <utility>

#include "wvbridge/native_bridge.h"

namespace wvbridge {

struct WebViewEvents {
    WebKitWebView* webview = nullptr;
    WebKitBackForwardList* back_forward_list = nullptr;
    jlong pointer = 0;
    const std::atomic_bool* closing = nullptr;

    gulong url_changed = 0;
    gulong load_changed = 0;
    gulong progress_changed = 0;
    gulong load_failed = 0;
    gulong history_changed = 0;
    gulong decide_policy = 0;
    gulong web_process_terminated = 0;

    bool last_load_failed = false;
    std::string last_error_reason;
};

namespace {

bool is_closing(const WebViewEvents* events) {
    return events != nullptr &&
           events->closing != nullptr &&
           events->closing->load(std::memory_order_acquire);
}

float clamp01(double value) {
    if (value != value || value < 0.0) return 0.0f;
    if (value > 1.0) return 1.0f;
    return static_cast<float>(value);
}

void notify_history(WebViewEvents* events) {
    if (!events || !events->webview || is_closing(events)) return;
    notify_can_go_back_change_to_jvm(
        events->pointer,
        webkit_web_view_can_go_back(events->webview) ? JNI_TRUE : JNI_FALSE
    );
    notify_can_go_forward_change_to_jvm(
        events->pointer,
        webkit_web_view_can_go_forward(events->webview) ? JNI_TRUE : JNI_FALSE
    );
}

void url_changed_cb(WebKitWebView* webview, GParamSpec*, gpointer user_data) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || !webview || is_closing(events)) return;
    notify_url_change_to_jvm(events->pointer, webkit_web_view_get_uri(webview));
}

void load_changed_cb(WebKitWebView* webview, WebKitLoadEvent load_event, gpointer user_data) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || !webview || is_closing(events)) return;

    switch (load_event) {
        case WEBKIT_LOAD_STARTED:
            events->last_load_failed = false;
            events->last_error_reason.clear();
            notify_page_loading_start_to_jvm(events->pointer, webkit_web_view_get_uri(webview));
            notify_page_loading_progress_to_jvm(events->pointer, 0.0f);
            break;
        case WEBKIT_LOAD_COMMITTED:
            notify_page_loading_progress_to_jvm(
                events->pointer,
                std::max(clamp01(webkit_web_view_get_estimated_load_progress(webview)), 0.1f)
            );
            break;
        case WEBKIT_LOAD_FINISHED: {
            notify_page_loading_progress_to_jvm(events->pointer, 1.0f);
            const bool success = !events->last_load_failed;
            notify_page_loading_end_to_jvm(
                events->pointer,
                success ? JNI_TRUE : JNI_FALSE,
                success ? nullptr : events->last_error_reason.c_str()
            );
            break;
        }
        case WEBKIT_LOAD_REDIRECTED:
            break;
    }
}

void progress_changed_cb(GObject* object, GParamSpec*, gpointer user_data) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || is_closing(events)) return;
    notify_page_loading_progress_to_jvm(
        events->pointer,
        clamp01(webkit_web_view_get_estimated_load_progress(WEBKIT_WEB_VIEW(object)))
    );
}

gboolean load_failed_cb(
    WebKitWebView*,
    WebKitLoadEvent,
    const gchar* failing_uri,
    GError* error,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || is_closing(events)) return FALSE;

    events->last_load_failed = true;
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
    events->last_error_reason = std::move(reason);
    return FALSE;
}

void web_process_terminated_cb(
    WebKitWebView*,
    WebKitWebProcessTerminationReason reason,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || is_closing(events)) return;

    const char* cause = nullptr;
    switch (reason) {
        case WEBKIT_WEB_PROCESS_CRASHED:
            cause = "WEBKIT_WEB_PROCESS_CRASHED";
            break;
        case WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT:
            cause = "WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT";
            break;
        case WEBKIT_WEB_PROCESS_TERMINATED_BY_API:
            cause = nullptr;
            break;
        default:
            cause = "WEBKIT_WEB_PROCESS_TERMINATION_REASON_UNKNOWN";
            break;
    }
    notify_webview_fatal_error_to_jvm(events->pointer, cause);
}

void history_changed_cb(
    WebKitBackForwardList*,
    WebKitBackForwardListItem*,
    GList*,
    gpointer user_data
) {
    notify_history(static_cast<WebViewEvents*>(user_data));
}

gboolean decide_policy_cb(
    WebKitWebView* webview,
    WebKitPolicyDecision* decision,
    WebKitPolicyDecisionType type,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    if (!events || !webview || !decision) return FALSE;

    if (is_closing(events)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        auto* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* action =
            webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
        if (action) {
            WebKitURIRequest* request = webkit_navigation_action_get_request(action);
            if (request) webkit_web_view_load_request(webview, request);
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

} // namespace

WebViewEvents* webview_events_create(
    WebKitWebView* webview,
    jlong pointer,
    const std::atomic_bool* closing
) {
    if (!webview) return nullptr;

    auto* events = new WebViewEvents();
    events->webview = webview;
    events->pointer = pointer;
    events->closing = closing;
    events->back_forward_list = webkit_web_view_get_back_forward_list(webview);

    events->url_changed =
        g_signal_connect(webview, "notify::uri", G_CALLBACK(url_changed_cb), events);
    events->load_changed =
        g_signal_connect(webview, "load-changed", G_CALLBACK(load_changed_cb), events);
    events->progress_changed =
        g_signal_connect(webview, "notify::estimated-load-progress", G_CALLBACK(progress_changed_cb), events);
    events->load_failed =
        g_signal_connect(webview, "load-failed", G_CALLBACK(load_failed_cb), events);
    events->decide_policy =
        g_signal_connect(webview, "decide-policy", G_CALLBACK(decide_policy_cb), events);
    events->web_process_terminated =
        g_signal_connect(webview, "web-process-terminated", G_CALLBACK(web_process_terminated_cb), events);

    if (events->back_forward_list) {
        events->history_changed = g_signal_connect(
            events->back_forward_list,
            "changed",
            G_CALLBACK(history_changed_cb),
            events
        );
        notify_history(events);
    }
    return events;
}

void webview_events_destroy(WebViewEvents* events) {
    if (!events) return;

    if (events->webview) {
        const gulong handlers[] = {
            events->url_changed,
            events->load_changed,
            events->progress_changed,
            events->load_failed,
            events->decide_policy,
            events->web_process_terminated
        };
        for (gulong handler : handlers) {
            if (handler != 0) g_signal_handler_disconnect(events->webview, handler);
        }
    }
    if (events->back_forward_list && events->history_changed != 0) {
        g_signal_handler_disconnect(events->back_forward_list, events->history_changed);
    }
    delete events;
}

} // namespace wvbridge

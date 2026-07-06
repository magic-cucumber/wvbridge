#include "webview_events.h"

#include <glib.h>

#include <algorithm>
#include <string>
#include <utility>

#include "wvbridge/native_bridge.h"
#include <wvbridge/logger.h>

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
    LOGGER_V("is_closing: checking close state");
    return events != nullptr &&
           events->closing != nullptr &&
           events->closing->load(std::memory_order_acquire);
}

float clamp01(double value) {
    LOGGER_V("clamp01: value=%f", value);
    if (value != value || value < 0.0) return 0.0f;
    if (value > 1.0) return 1.0f;
    return static_cast<float>(value);
}

std::string normalize_url_for_compare(const char* value) {
    std::string result = value != nullptr ? value : "";
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    return result;
}

bool apply_navigation_interceptor(WebViewEvents* events, WebKitPolicyDecision* decision, const char* uri) {
    LOGGER_I(
        "apply_navigation_interceptor: events=%p webview=%p uri=%s",
        events,
        events ? events->webview : nullptr,
        uri ? uri : ""
    );
    if (!events || !events->webview || !decision) {
        LOGGER_W("apply_navigation_interceptor: missing events/webview/decision, allowing navigation");
        return false;
    }

    char* result = notify_navigation_interceptor_to_jvm(events->pointer, uri != nullptr ? uri : "");
    if (result == nullptr) {
        LOGGER_W("apply_navigation_interceptor: no JVM result, allowing navigation");
        return false;
    }

    const char action = result[0];
    if (action == '2') {
        LOGGER_I("apply_navigation_interceptor: decision=cancel uri=%s", uri ? uri : "");
        webkit_policy_decision_ignore(decision);
        free_navigation_interceptor_result(result);
        return true;
    }
    if (action == '3') {
        char* redirect_uri = g_uri_unescape_string(result + 1, nullptr);
        const char* current_uri = webkit_web_view_get_uri(events->webview);
        LOGGER_I(
            "apply_navigation_interceptor: decision=redirect uri=%s current=%s redirect=%s",
            uri ? uri : "",
            current_uri ? current_uri : "",
            redirect_uri ? redirect_uri : ""
        );
        webkit_policy_decision_ignore(decision);
        if (redirect_uri != nullptr && redirect_uri[0] != '\0') {
            LOGGER_I("apply_navigation_interceptor: notifying JVM url change for redirect=%s", redirect_uri);
            notify_url_change_to_jvm(events->pointer, redirect_uri);

            const bool same_uri = normalize_url_for_compare(current_uri) == normalize_url_for_compare(redirect_uri);
            if (same_uri) {
                LOGGER_I("apply_navigation_interceptor: redirect matches current uri, reloading");
                webkit_web_view_reload(events->webview);
            } else {
                LOGGER_I("apply_navigation_interceptor: loading redirect uri");
                webkit_web_view_load_uri(events->webview, redirect_uri);
            }
        } else {
            LOGGER_W("apply_navigation_interceptor: redirect uri is empty");
        }
        if (redirect_uri != nullptr) g_free(redirect_uri);
        free_navigation_interceptor_result(result);
        return true;
    }

    LOGGER_V("apply_navigation_interceptor: decision=allow uri=%s result=%s", uri ? uri : "", result);
    free_navigation_interceptor_result(result);
    return false;
}

void notify_history(WebViewEvents* events) {
    LOGGER_I("notify_history: pointer=%ld", events ? events->pointer : 0);
    if (!events || !events->webview || is_closing(events)) {
        LOGGER_W("notify_history: null events/webview or closing, aborting");
        return;
    }
    LOGGER_V("notify_history: notifying can_go_back/forward to JVM");
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
    LOGGER_I("url_changed_cb: webview=%p, events=%p", webview, events);
    if (!events || !webview || is_closing(events)) {
        LOGGER_W("url_changed_cb: null webview/events or closing, aborting");
        return;
    }
    LOGGER_V("url_changed_cb: notifying JVM of URL=%s", webkit_web_view_get_uri(webview));
    notify_url_change_to_jvm(events->pointer, webkit_web_view_get_uri(webview));
}

void load_changed_cb(WebKitWebView* webview, WebKitLoadEvent load_event, gpointer user_data) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    LOGGER_I("load_changed_cb: webview=%p, load_event=%d, events=%p", webview, (int)load_event, events);
    if (!events || !webview || is_closing(events)) {
        LOGGER_W("load_changed_cb: null webview/events or closing, aborting");
        return;
    }

    switch (load_event) {
        case WEBKIT_LOAD_STARTED:
            LOGGER_V("load_changed_cb: WEBKIT_LOAD_STARTED, uri=%s", webkit_web_view_get_uri(webview));
            events->last_load_failed = false;
            events->last_error_reason.clear();
            notify_page_loading_start_to_jvm(events->pointer, webkit_web_view_get_uri(webview));
            notify_page_loading_progress_to_jvm(events->pointer, 0.0f);
            break;
        case WEBKIT_LOAD_COMMITTED:
            LOGGER_V("load_changed_cb: WEBKIT_LOAD_COMMITTED");
            notify_page_loading_progress_to_jvm(
                events->pointer,
                std::max(clamp01(webkit_web_view_get_estimated_load_progress(webview)), 0.1f)
            );
            break;
        case WEBKIT_LOAD_FINISHED: {
            LOGGER_V("load_changed_cb: WEBKIT_LOAD_FINISHED");
            notify_page_loading_progress_to_jvm(events->pointer, 1.0f);
            const bool success = !events->last_load_failed;
            LOGGER_V("load_changed_cb: success=%d", success ? 1 : 0);
            notify_page_loading_end_to_jvm(
                events->pointer,
                success ? JNI_TRUE : JNI_FALSE,
                success ? nullptr : events->last_error_reason.c_str()
            );
            break;
        }
        case WEBKIT_LOAD_REDIRECTED:
            LOGGER_V("load_changed_cb: WEBKIT_LOAD_REDIRECTED");
            break;
    }
}

void progress_changed_cb(GObject* object, GParamSpec*, gpointer user_data) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    LOGGER_I("progress_changed_cb: object=%p, events=%p", object, events);
    if (!events || is_closing(events)) {
        LOGGER_W("progress_changed_cb: null events or closing, aborting");
        return;
    }
    float progress = clamp01(webkit_web_view_get_estimated_load_progress(WEBKIT_WEB_VIEW(object)));
    LOGGER_V("progress_changed_cb: notifying progress=%.2f", progress);
    notify_page_loading_progress_to_jvm(
        events->pointer,
        progress
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
    LOGGER_I("load_failed_cb: failing_uri=%s, events=%p", failing_uri ? failing_uri : "null", events);
    if (!events || is_closing(events)) {
        LOGGER_W("load_failed_cb: null events or closing, aborting");
        return FALSE;
    }

    events->last_load_failed = true;
    std::string reason = "webkitgtk.load-failed";
    if (error) {
        LOGGER_V("load_failed_cb: building error from domain/code/message");
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
    LOGGER_V("load_failed_cb: reason=%s", reason.c_str());
    events->last_error_reason = std::move(reason);
    return FALSE;
}

void web_process_terminated_cb(
    WebKitWebView*,
    WebKitWebProcessTerminationReason reason,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    LOGGER_I("web_process_terminated_cb: reason=%d, events=%p", (int)reason, events);
    if (!events || is_closing(events)) {
        LOGGER_W("web_process_terminated_cb: null events or closing, aborting");
        return;
    }

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
    LOGGER_V("web_process_terminated_cb: cause=%s", cause ? cause : "null");
    notify_webview_fatal_error_to_jvm(events->pointer, cause);
}

void history_changed_cb(
    WebKitBackForwardList*,
    WebKitBackForwardListItem*,
    GList*,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    LOGGER_I("history_changed_cb: events=%p", events);
    LOGGER_V("history_changed_cb: delegating to notify_history");
    notify_history(events);
}

gboolean decide_policy_cb(
    WebKitWebView* webview,
    WebKitPolicyDecision* decision,
    WebKitPolicyDecisionType type,
    gpointer user_data
) {
    auto* events = static_cast<WebViewEvents*>(user_data);
    LOGGER_I("decide_policy_cb: webview=%p, decision=%p, type=%d, events=%p", webview, decision, (int)type, events);
    if (!events || !webview || !decision) {
        LOGGER_W("decide_policy_cb: null webview/events/decision, aborting");
        return FALSE;
    }

    if (is_closing(events)) {
        LOGGER_V("decide_policy_cb: closing, ignoring decision");
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        LOGGER_V("decide_policy_cb: NEW_WINDOW_ACTION, loading request in same webview");
        auto* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* action =
            webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
        if (action) {
            WebKitURIRequest* request = webkit_navigation_action_get_request(action);
            if (request) {
                const char* uri = webkit_uri_request_get_uri(request);
                if (apply_navigation_interceptor(events, decision, uri)) return TRUE;
                webkit_web_view_load_request(webview, request);
            }
        }
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }

    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        LOGGER_V("decide_policy_cb: NAVIGATION_ACTION, using decision");
        auto* navigation_decision = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
        WebKitNavigationAction* action =
            webkit_navigation_policy_decision_get_navigation_action(navigation_decision);
        if (action) {
            WebKitURIRequest* request = webkit_navigation_action_get_request(action);
            if (request && apply_navigation_interceptor(events, decision, webkit_uri_request_get_uri(request))) {
                return TRUE;
            }
        }
        webkit_policy_decision_use(decision);
        return TRUE;
    }
    LOGGER_V("decide_policy_cb: unhandled decision type=%d, returning FALSE", (int)type);
    return FALSE;
}

} // namespace

WebViewEvents* webview_events_create(
    WebKitWebView* webview,
    jlong pointer,
    const std::atomic_bool* closing
) {
    LOGGER_I("webview_events_create: webview=%p, pointer=%ld", webview, pointer);
    if (!webview) {
        LOGGER_W("webview_events_create: null webview, aborting");
        return nullptr;
    }

    auto* events = new WebViewEvents();
    events->webview = webview;
    events->pointer = pointer;
    events->closing = closing;
    events->back_forward_list = webkit_web_view_get_back_forward_list(webview);

    LOGGER_V("webview_events_create: connecting signals");
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
        LOGGER_V("webview_events_create: connecting back_forward_list changed signal");
        events->history_changed = g_signal_connect(
            events->back_forward_list,
            "changed",
            G_CALLBACK(history_changed_cb),
            events
        );
        notify_history(events);
    }
    LOGGER_V("webview_events_create: done, events=%p", events);
    return events;
}

void webview_events_destroy(WebViewEvents* events) {
    LOGGER_I("webview_events_destroy: events=%p", events);
    if (!events) {
        LOGGER_W("webview_events_destroy: null events, aborting");
        return;
    }

    if (events->webview) {
        LOGGER_V("webview_events_destroy: disconnecting webview signal handlers");
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
        LOGGER_V("webview_events_destroy: disconnecting back_forward_list signal handler");
        g_signal_handler_disconnect(events->back_forward_list, events->history_changed);
    }
    LOGGER_V("webview_events_destroy: deleting events");
    delete events;
}

} // namespace wvbridge

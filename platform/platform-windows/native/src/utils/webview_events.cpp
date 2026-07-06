#include "webview_events.h"

#include "webview2_callback.h"
#include "wvbridge/native_bridge.h"

#include <string>

#include <wvbridge/logger.h>

using Microsoft::WRL::ComPtr;

struct WebViewEvents {
    ComPtr<ICoreWebView2> webview;

    EventRegistrationToken source_changed{};
    EventRegistrationToken navigation_starting{};
    EventRegistrationToken content_loading{};
    EventRegistrationToken navigation_completed{};
    EventRegistrationToken history_changed{};
    EventRegistrationToken new_window_requested{};
    EventRegistrationToken process_failed{};

    bool has_source_changed = false;
    bool has_navigation_starting = false;
    bool has_content_loading = false;
    bool has_navigation_completed = false;
    bool has_history_changed = false;
    bool has_new_window_requested = false;
    bool has_process_failed = false;
};

namespace {

constexpr float kPhaseStarted = 0.0f;
constexpr float kPhaseContentLoading = 0.5f;
constexpr float kPhaseCompleted = 1.0f;

int hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

std::string percent_decode(const char* value) {
    if (value == nullptr) return {};
    std::string result;
    for (size_t index = 0; value[index] != '\0'; ++index) {
        if (value[index] == '%' && value[index + 1] != '\0' && value[index + 2] != '\0') {
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high >= 0 && low >= 0) {
                result.push_back(static_cast<char>((high << 4) | low));
                index += 2;
                continue;
            }
        }
        result.push_back(value[index] == '+' ? ' ' : value[index]);
    }
    return result;
}

std::wstring utf8_to_wstring(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) return {};
    std::wstring result(static_cast<size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::wstring current_webview_source(WebViewContext* ctx) {
    if (ctx == nullptr || !ctx->webview) return {};
    LPWSTR source = nullptr;
    std::wstring result;
    if (SUCCEEDED(ctx->webview->get_Source(&source)) && source != nullptr) {
        result = source;
        CoTaskMemFree(source);
    }
    return result;
}

std::wstring normalize_url_for_compare(std::wstring value) {
    while (value.size() > 1 && value.back() == L'/') {
        value.pop_back();
    }
    return value;
}

bool apply_navigation_interceptor(
    WebViewContext* ctx,
    ICoreWebView2NavigationStartingEventArgs* args,
    const wchar_t* uri
) {
    LOGGER_I("apply_navigation_interceptor: ctx=%p uri=%ls", ctx, uri != nullptr ? uri : L"");
    if (!ctx || !args) {
        LOGGER_W("apply_navigation_interceptor: missing ctx or navigation args, allowing navigation");
        return false;
    }

    char* result = notify_navigation_interceptor_to_jvm(
        reinterpret_cast<jlong>(ctx),
        uri != nullptr ? uri : L""
    );
    if (result == nullptr) {
        LOGGER_W("apply_navigation_interceptor: no JVM result, allowing navigation");
        return false;
    }

    const char action = result[0];
    if (action == '2') {
        LOGGER_I("apply_navigation_interceptor: decision=cancel uri=%ls", uri != nullptr ? uri : L"");
        args->put_Cancel(TRUE);
        free_navigation_interceptor_result(result);
        return true;
    }
    if (action == '3') {
        const std::wstring redirect_url = utf8_to_wstring(percent_decode(result + 1));
        const std::wstring current_url = current_webview_source(ctx);
        LOGGER_I(
            "apply_navigation_interceptor: decision=redirect uri=%ls current=%ls redirect=%ls",
            uri != nullptr ? uri : L"",
            current_url.c_str(),
            redirect_url.c_str()
        );
        args->put_Cancel(TRUE);
        if (!redirect_url.empty() && ctx->webview) {
            LOGGER_I("apply_navigation_interceptor: notifying JVM url change for redirect=%ls", redirect_url.c_str());
            notify_url_change_to_jvm(reinterpret_cast<jlong>(ctx), redirect_url.c_str());
            if (normalize_url_for_compare(redirect_url) == normalize_url_for_compare(current_url)) {
                LOGGER_I("apply_navigation_interceptor: redirect matches current source, reloading");
                ctx->webview->Reload();
            } else {
                LOGGER_I("apply_navigation_interceptor: navigating to redirect target");
                ctx->webview->Navigate(redirect_url.c_str());
            }
        } else {
            LOGGER_W("apply_navigation_interceptor: redirect target is empty or webview unavailable");
        }
        free_navigation_interceptor_result(result);
        return true;
    }

    LOGGER_V("apply_navigation_interceptor: decision=allow uri=%ls result=%s", uri != nullptr ? uri : L"", result);
    free_navigation_interceptor_result(result);
    return false;
}

std::wstring format_failure_reason(ICoreWebView2NavigationCompletedEventArgs* args) {
    LOGGER_V("format_failure_reason");
    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
    if (!args || FAILED(args->get_WebErrorStatus(&status))) {
        LOGGER_W("format_failure_reason: null args or FAILED, returning default");
        return L"webview2.navigation.failed";
    }
    return L"webview2.navigation.failed: WebErrorStatus=" +
           std::to_wstring(static_cast<int>(status));
}

std::wstring format_named_constant(const wchar_t* name, int value) {
    LOGGER_V("format_named_constant: name=%ls, value=%d", name, value);
    return std::wstring(name) + L"(" + std::to_wstring(value) + L")";
}

std::wstring format_process_failed_kind(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
    LOGGER_V("format_process_failed_kind: kind=%d", static_cast<int>(kind));
    switch (kind) {
        case COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_FRAME_RENDER_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_UTILITY_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_SANDBOX_HELPER_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_GPU_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_GPU_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_PLUGIN_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_PLUGIN_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        case COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_BROKER_PROCESS_EXITED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_PPAPI_BROKER_PROCESS_EXITED",
                static_cast<int>(kind)
            );
        default:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_KIND_UNKNOWN",
                static_cast<int>(kind)
            );
    }
}

std::wstring format_process_failed_reason(COREWEBVIEW2_PROCESS_FAILED_REASON reason) {
    LOGGER_V("format_process_failed_reason: reason=%d", static_cast<int>(reason));
    switch (reason) {
        case COREWEBVIEW2_PROCESS_FAILED_REASON_UNEXPECTED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_UNEXPECTED",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_UNRESPONSIVE:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_UNRESPONSIVE",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_TERMINATED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_TERMINATED",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_CRASHED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_CRASHED",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_LAUNCH_FAILED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_LAUNCH_FAILED",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_OUT_OF_MEMORY:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_OUT_OF_MEMORY",
                static_cast<int>(reason)
            );
        case COREWEBVIEW2_PROCESS_FAILED_REASON_PROFILE_DELETED:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_PROFILE_DELETED",
                static_cast<int>(reason)
            );
        default:
            return format_named_constant(
                L"COREWEBVIEW2_PROCESS_FAILED_REASON_UNKNOWN",
                static_cast<int>(reason)
            );
    }
}

std::wstring format_process_failed_message(ICoreWebView2ProcessFailedEventArgs* args) {
    LOGGER_V("format_process_failed_message");
    COREWEBVIEW2_PROCESS_FAILED_KIND kind =
        COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
    if (args) {
        args->get_ProcessFailedKind(&kind);
    }

    std::wstring message = L"webview2.process.failed: Kind=" +
                           format_process_failed_kind(kind);

    ComPtr<ICoreWebView2ProcessFailedEventArgs2> args2;
    if (!args || FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))) || !args2) {
        LOGGER_V("format_process_failed_message: no ICoreWebView2ProcessFailedEventArgs2 available");
        return message;
    }

    COREWEBVIEW2_PROCESS_FAILED_REASON reason =
        COREWEBVIEW2_PROCESS_FAILED_REASON_UNEXPECTED;
    if (SUCCEEDED(args2->get_Reason(&reason))) {
        message += L"; Reason=" + format_process_failed_reason(reason);
    }

    int exit_code = 0;
    if (SUCCEEDED(args2->get_ExitCode(&exit_code))) {
        message += L"; ExitCode=" + std::to_wstring(exit_code);
    }

    LPWSTR process_description = nullptr;
    if (SUCCEEDED(args2->get_ProcessDescription(&process_description)) &&
        process_description) {
        if (process_description[0] != L'\0') {
            message += L"; ProcessDescription=";
            message += process_description;
        }
        CoTaskMemFree(process_description);
    }

    return message;
}

bool can_notify(WebViewContext* ctx) {
    LOGGER_V("can_notify: ctx=%p", ctx);
    return ctx != nullptr && !ctx->closing.load(std::memory_order_acquire);
}

void notify_history(WebViewContext* ctx) {
    LOGGER_V("notify_history: ctx=%p", ctx);
    if (!can_notify(ctx) || !ctx->webview) {
        LOGGER_W("notify_history: cannot notify, aborting");
        return;
    }

    BOOL can_go_back = FALSE;
    BOOL can_go_forward = FALSE;
    ctx->webview->get_CanGoBack(&can_go_back);
    ctx->webview->get_CanGoForward(&can_go_forward);

    const jlong pointer = reinterpret_cast<jlong>(ctx);
    notify_can_go_back_change_to_jvm(pointer, can_go_back == TRUE ? JNI_TRUE : JNI_FALSE);
    notify_can_go_forward_change_to_jvm(pointer, can_go_forward == TRUE ? JNI_TRUE : JNI_FALSE);
}

} // namespace

WebViewEvents* webview_events_create(WebViewContext* ctx) {
    LOGGER_I("webview_events_create: ctx=%p", ctx);
    if (!ctx || !ctx->webview) {
        LOGGER_W("webview_events_create: null ctx or webview, aborting");
        return nullptr;
    }

    auto* events = new WebViewEvents();
    events->webview = ctx->webview;

    events->has_source_changed = SUCCEEDED(events->webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [ctx](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
                LOGGER_V("webview_events: SourceChanged callback");
                if (!can_notify(ctx) || !sender) return S_OK;
                LPWSTR source = nullptr;
                if (SUCCEEDED(sender->get_Source(&source)) && source) {
                    notify_url_change_to_jvm(reinterpret_cast<jlong>(ctx), source);
                    CoTaskMemFree(source);
                }
                return S_OK;
            }
        ).Get(),
        &events->source_changed
    ));
    LOGGER_V("webview_events_create: SourceChanged registered=%d", events->has_source_changed);

    events->has_navigation_starting = SUCCEEDED(events->webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                LOGGER_V("webview_events: NavigationStarting callback");
                if (!can_notify(ctx)) return S_OK;
                LPWSTR uri = nullptr;
                if (args && SUCCEEDED(args->get_Uri(&uri)) && uri) {
                    if (apply_navigation_interceptor(ctx, args, uri)) {
                        CoTaskMemFree(uri);
                        return S_OK;
                    }
                    notify_page_loading_start_to_jvm(reinterpret_cast<jlong>(ctx), uri);
                    CoTaskMemFree(uri);
                } else {
                    if (apply_navigation_interceptor(ctx, args, L"")) return S_OK;
                    notify_page_loading_start_to_jvm(reinterpret_cast<jlong>(ctx), L"");
                }
                notify_page_loading_progress_to_jvm(
                    reinterpret_cast<jlong>(ctx),
                    kPhaseStarted
                );
                return S_OK;
            }
        ).Get(),
        &events->navigation_starting
    ));
    LOGGER_V("webview_events_create: NavigationStarting registered=%d", events->has_navigation_starting);

    events->has_content_loading = SUCCEEDED(events->webview->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2ContentLoadingEventArgs*) -> HRESULT {
                LOGGER_V("webview_events: ContentLoading callback");
                if (can_notify(ctx)) {
                    notify_page_loading_progress_to_jvm(
                        reinterpret_cast<jlong>(ctx),
                        kPhaseContentLoading
                    );
                }
                return S_OK;
            }
        ).Get(),
        &events->content_loading
    ));
    LOGGER_V("webview_events_create: ContentLoading registered=%d", events->has_content_loading);

    events->has_navigation_completed = SUCCEEDED(events->webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                LOGGER_V("webview_events: NavigationCompleted callback");
                if (!can_notify(ctx)) return S_OK;

                BOOL success = FALSE;
                std::wstring reason;
                if (args) {
                    args->get_IsSuccess(&success);
                    if (success != TRUE) reason = format_failure_reason(args);
                }

                const jlong pointer = reinterpret_cast<jlong>(ctx);
                notify_page_loading_progress_to_jvm(pointer, kPhaseCompleted);
                notify_page_loading_end_to_jvm(
                    pointer,
                    success == TRUE ? JNI_TRUE : JNI_FALSE,
                    success == TRUE ? nullptr : reason.c_str()
                );
                return S_OK;
            }
        ).Get(),
        &events->navigation_completed
    ));
    LOGGER_V("webview_events_create: NavigationCompleted registered=%d", events->has_navigation_completed);

    events->has_history_changed = SUCCEEDED(events->webview->add_HistoryChanged(
        Callback<ICoreWebView2HistoryChangedEventHandler>(
            [ctx](ICoreWebView2*, IUnknown*) -> HRESULT {
                LOGGER_V("webview_events: HistoryChanged callback");
                notify_history(ctx);
                return S_OK;
            }
        ).Get(),
        &events->history_changed
    ));
    LOGGER_V("webview_events_create: HistoryChanged registered=%d", events->has_history_changed);

    events->has_new_window_requested = SUCCEEDED(events->webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                LOGGER_V("webview_events: NewWindowRequested callback");
                if (!ctx || !ctx->webview || !args) return S_OK;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                    LOGGER_V("webview_events: NewWindowRequested navigating to uri");
                    ctx->webview->Navigate(uri);
                    CoTaskMemFree(uri);
                }
                args->put_Handled(TRUE);
                return S_OK;
            }
        ).Get(),
        &events->new_window_requested
    ));
    LOGGER_V("webview_events_create: NewWindowRequested registered=%d", events->has_new_window_requested);

    events->has_process_failed = SUCCEEDED(events->webview->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
                LOGGER_V("webview_events: ProcessFailed callback");
                if (!can_notify(ctx)) return S_OK;

                const std::wstring cause = format_process_failed_message(args);
                notify_webview_fatal_error_to_jvm(
                    reinterpret_cast<jlong>(ctx),
                    cause.c_str()
                );
                return S_OK;
            }
        ).Get(),
        &events->process_failed
    ));
    LOGGER_V("webview_events_create: ProcessFailed registered=%d", events->has_process_failed);

    notify_history(ctx);
    LOGGER_V("webview_events_create: all events registered, returning events=%p", events);
    return events;
}

void webview_events_destroy(WebViewEvents* events) {
    LOGGER_I("webview_events_destroy: events=%p", events);
    if (!events) {
        LOGGER_W("webview_events_destroy: null events, aborting");
        return;
    }

    if (events->webview) {
        if (events->has_source_changed) {
            LOGGER_V("webview_events_destroy: removing SourceChanged");
            events->webview->remove_SourceChanged(events->source_changed);
        }
        if (events->has_navigation_starting) {
            LOGGER_V("webview_events_destroy: removing NavigationStarting");
            events->webview->remove_NavigationStarting(events->navigation_starting);
        }
        if (events->has_content_loading) {
            LOGGER_V("webview_events_destroy: removing ContentLoading");
            events->webview->remove_ContentLoading(events->content_loading);
        }
        if (events->has_navigation_completed) {
            LOGGER_V("webview_events_destroy: removing NavigationCompleted");
            events->webview->remove_NavigationCompleted(events->navigation_completed);
        }
        if (events->has_history_changed) {
            LOGGER_V("webview_events_destroy: removing HistoryChanged");
            events->webview->remove_HistoryChanged(events->history_changed);
        }
        if (events->has_new_window_requested) {
            LOGGER_V("webview_events_destroy: removing NewWindowRequested");
            events->webview->remove_NewWindowRequested(events->new_window_requested);
        }
        if (events->has_process_failed) {
            LOGGER_V("webview_events_destroy: removing ProcessFailed");
            events->webview->remove_ProcessFailed(events->process_failed);
        }
    }
    delete events;
    LOGGER_V("webview_events_destroy: events destroyed");
}

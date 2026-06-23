#include "webview_events.h"

#include "webview2_callback.h"
#include "wvbridge/native_bridge.h"

#include <string>

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

std::wstring format_failure_reason(ICoreWebView2NavigationCompletedEventArgs* args) {
    COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
    if (!args || FAILED(args->get_WebErrorStatus(&status))) {
        return L"webview2.navigation.failed";
    }
    return L"webview2.navigation.failed: WebErrorStatus=" +
           std::to_wstring(static_cast<int>(status));
}

std::wstring format_named_constant(const wchar_t* name, int value) {
    return std::wstring(name) + L"(" + std::to_wstring(value) + L")";
}

std::wstring format_process_failed_kind(COREWEBVIEW2_PROCESS_FAILED_KIND kind) {
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
    COREWEBVIEW2_PROCESS_FAILED_KIND kind =
        COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED;
    if (args) {
        args->get_ProcessFailedKind(&kind);
    }

    std::wstring message = L"webview2.process.failed: Kind=" +
                           format_process_failed_kind(kind);

    ComPtr<ICoreWebView2ProcessFailedEventArgs2> args2;
    if (!args || FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))) || !args2) {
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
    return ctx != nullptr && !ctx->closing.load(std::memory_order_acquire);
}

void notify_history(WebViewContext* ctx) {
    if (!can_notify(ctx) || !ctx->webview) return;

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
    if (!ctx || !ctx->webview) return nullptr;

    auto* events = new WebViewEvents();
    events->webview = ctx->webview;

    events->has_source_changed = SUCCEEDED(events->webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [ctx](ICoreWebView2* sender, ICoreWebView2SourceChangedEventArgs*) -> HRESULT {
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

    events->has_navigation_starting = SUCCEEDED(events->webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                if (!can_notify(ctx)) return S_OK;
                LPWSTR uri = nullptr;
                if (args && SUCCEEDED(args->get_Uri(&uri)) && uri) {
                    notify_page_loading_start_to_jvm(reinterpret_cast<jlong>(ctx), uri);
                    CoTaskMemFree(uri);
                } else {
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

    events->has_content_loading = SUCCEEDED(events->webview->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2ContentLoadingEventArgs*) -> HRESULT {
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

    events->has_navigation_completed = SUCCEEDED(events->webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
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

    events->has_history_changed = SUCCEEDED(events->webview->add_HistoryChanged(
        Callback<ICoreWebView2HistoryChangedEventHandler>(
            [ctx](ICoreWebView2*, IUnknown*) -> HRESULT {
                notify_history(ctx);
                return S_OK;
            }
        ).Get(),
        &events->history_changed
    ));

    events->has_new_window_requested = SUCCEEDED(events->webview->add_NewWindowRequested(
        Callback<ICoreWebView2NewWindowRequestedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                if (!ctx || !ctx->webview || !args) return S_OK;
                LPWSTR uri = nullptr;
                if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                    ctx->webview->Navigate(uri);
                    CoTaskMemFree(uri);
                }
                args->put_Handled(TRUE);
                return S_OK;
            }
        ).Get(),
        &events->new_window_requested
    ));

    events->has_process_failed = SUCCEEDED(events->webview->add_ProcessFailed(
        Callback<ICoreWebView2ProcessFailedEventHandler>(
            [ctx](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
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

    notify_history(ctx);
    return events;
}

void webview_events_destroy(WebViewEvents* events) {
    if (!events) return;

    if (events->webview) {
        if (events->has_source_changed) {
            events->webview->remove_SourceChanged(events->source_changed);
        }
        if (events->has_navigation_starting) {
            events->webview->remove_NavigationStarting(events->navigation_starting);
        }
        if (events->has_content_loading) {
            events->webview->remove_ContentLoading(events->content_loading);
        }
        if (events->has_navigation_completed) {
            events->webview->remove_NavigationCompleted(events->navigation_completed);
        }
        if (events->has_history_changed) {
            events->webview->remove_HistoryChanged(events->history_changed);
        }
        if (events->has_new_window_requested) {
            events->webview->remove_NewWindowRequested(events->new_window_requested);
        }
        if (events->has_process_failed) {
            events->webview->remove_ProcessFailed(events->process_failed);
        }
    }
    delete events;
}

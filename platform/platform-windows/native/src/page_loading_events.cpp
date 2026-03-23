#include "page_loading_events.h"

#include "java_listener.h"
#include "webview2_callback.h"

#include <string>

namespace {

constexpr float kPhaseStarted = 0.0f;
constexpr float kPhaseContentLoading = 0.5f;
constexpr float kPhaseCompleted = 1.0f;

std::wstring format_failure_reason(ICoreWebView2NavigationCompletedEventArgs *args) {
    COREWEBVIEW2_WEB_ERROR_STATUS error_status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
    if (!args || FAILED(args->get_WebErrorStatus(&error_status))) {
        return L"webview2.navigation.failed";
    }
    return L"webview2.navigation.failed: WebErrorStatus=" + std::to_wstring(static_cast<int>(error_status));
}

} // namespace

void register_page_loading_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview) return;

    unregister_page_loading_events(ctx);

    ctx->webview->add_NavigationStarting(
        Callback<ICoreWebView2NavigationStartingEventHandler>(
            [ctx](ICoreWebView2 *, ICoreWebView2NavigationStartingEventArgs *args) -> HRESULT {
                if (!ctx || ctx->closing.load(std::memory_order_acquire)) return S_OK;

                LPWSTR uri = nullptr;
                if (args && SUCCEEDED(args->get_Uri(&uri)) && uri) {
                    notify_string(ctx, ctx->page_loading_start_listener, uri);
                    CoTaskMemFree(uri);
                } else {
                    notify_string(ctx, ctx->page_loading_start_listener, L"");
                }
                notify_float(ctx, ctx->page_loading_progress_listener, kPhaseStarted);
                return S_OK;
            }
        ).Get(),
        &ctx->token_nav_starting
    );
    ctx->nav_starting_registered = true;

    ctx->webview->add_ContentLoading(
        Callback<ICoreWebView2ContentLoadingEventHandler>(
            [ctx](ICoreWebView2 *, ICoreWebView2ContentLoadingEventArgs *) -> HRESULT {
                if (!ctx || ctx->closing.load(std::memory_order_acquire)) return S_OK;
                notify_float(ctx, ctx->page_loading_progress_listener, kPhaseContentLoading);
                return S_OK;
            }
        ).Get(),
        &ctx->token_content_loading
    );
    ctx->content_loading_registered = true;

    ctx->webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [ctx](ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *args) -> HRESULT {
                if (!ctx || ctx->closing.load(std::memory_order_acquire)) return S_OK;

                BOOL success = FALSE;
                std::wstring error_reason;
                if (args) {
                    args->get_IsSuccess(&success);
                    if (success != TRUE) {
                        error_reason = format_failure_reason(args);
                    }
                }

                notify_float(ctx, ctx->page_loading_progress_listener, kPhaseCompleted);
                notify_boolean_string(
                    ctx,
                    ctx->page_loading_end_listener,
                    success == TRUE,
                    success == TRUE ? nullptr : error_reason.c_str()
                );
                return S_OK;
            }
        ).Get(),
        &ctx->token_nav_completed
    );
    ctx->nav_completed_registered = true;
}

void unregister_page_loading_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview) return;

    if (ctx->nav_starting_registered) {
        ctx->webview->remove_NavigationStarting(ctx->token_nav_starting);
        ctx->nav_starting_registered = false;
    }
    if (ctx->content_loading_registered) {
        ctx->webview->remove_ContentLoading(ctx->token_content_loading);
        ctx->content_loading_registered = false;
    }
    if (ctx->nav_completed_registered) {
        ctx->webview->remove_NavigationCompleted(ctx->token_nav_completed);
        ctx->nav_completed_registered = false;
    }
}

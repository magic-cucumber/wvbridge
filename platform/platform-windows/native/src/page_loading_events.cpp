#include "page_loading_events.h"

#include "webview2_callback.h"
#include "wvbridge/java_runtime.h"
#include "wvbridge/utils.h"

#include <cwchar>
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

java_caller *retain_listener(JavaListenerState &state) {
    std::lock_guard<std::mutex> lock(state.mutex);
    return java_caller_retain(state.caller);
}

void notify_string(WebViewContext *ctx, JavaListenerState &state, const wchar_t *value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    java_caller *caller = retain_listener(state);
    if (!caller) return;

    int attached = 0;
    JNIEnv *env = java_runtime_get_env(&attached);
    if (!env) {
        java_caller_release(caller);
        return;
    }

    const wchar_t *safe_value = value ? value : L"";
    const jchar *chars = reinterpret_cast<const jchar *>(safe_value);
    const jsize len = static_cast<jsize>(wcslen(safe_value));
    jstring boxed = env->NewString(chars, len);
    if (boxed) {
        jvalue args[1];
        ZeroMemory(args, sizeof(args));
        args[0].l = boxed;
        java_caller_invoke(caller, args, nullptr);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

void notify_float(WebViewContext *ctx, JavaListenerState &state, float value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    java_caller *caller = retain_listener(state);
    if (!caller) return;

    jvalue boxed;
    if (java_caller_pack_float(value, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        ZeroMemory(args, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

void notify_boolean_string(WebViewContext *ctx, JavaListenerState &state, bool value, const wchar_t *message) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    java_caller *caller = retain_listener(state);
    if (!caller) return;

    jvalue boxed_bool;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxed_bool) != JAVA_CALLER_OK) {
        java_caller_release(caller);
        return;
    }

    int attached = 0;
    JNIEnv *env = java_runtime_get_env(&attached);
    if (!env) {
        java_caller_delete_global_ref(boxed_bool.l);
        java_caller_release(caller);
        return;
    }

    jstring boxed_message = nullptr;
    if (message) {
        const jchar *chars = reinterpret_cast<const jchar *>(message);
        const jsize len = static_cast<jsize>(wcslen(message));
        boxed_message = env->NewString(chars, len);
        if (!boxed_message) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            java_caller_delete_global_ref(boxed_bool.l);
            java_runtime_detach_env(attached);
            java_caller_release(caller);
            return;
        }
    }

    jvalue args[2];
    ZeroMemory(args, sizeof(args));
    args[0] = boxed_bool;
    args[1].l = boxed_message;
    java_caller_invoke(caller, args, nullptr);

    if (boxed_message) env->DeleteLocalRef(boxed_message);
    java_caller_delete_global_ref(boxed_bool.l);
    java_runtime_detach_env(attached);
    java_caller_release(caller);
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

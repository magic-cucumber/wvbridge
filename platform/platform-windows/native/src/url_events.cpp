#include "url_events.h"

#include <cwchar>

#include "webview2_callback.h"
#include "wvbridge/java_runtime.h"

namespace {

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

} // namespace

void register_url_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview) return;

    unregister_url_events(ctx);
    ctx->webview->add_SourceChanged(
        Callback<ICoreWebView2SourceChangedEventHandler>(
            [ctx](ICoreWebView2 *sender, ICoreWebView2SourceChangedEventArgs *) -> HRESULT {
                if (!ctx || ctx->closing.load(std::memory_order_acquire) || !sender) return S_OK;

                LPWSTR source = nullptr;
                if (SUCCEEDED(sender->get_Source(&source)) && source) {
                    notify_string(ctx, ctx->url_listener, source);
                    CoTaskMemFree(source);
                }
                return S_OK;
            }
        ).Get(),
        &ctx->token_source_changed
    );
    ctx->source_changed_registered = true;
}

void unregister_url_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview || !ctx->source_changed_registered) return;
    ctx->webview->remove_SourceChanged(ctx->token_source_changed);
    ctx->source_changed_registered = false;
}

#include "history_events.h"

#include "webview2_callback.h"
#include "wvbridge/utils.h"

namespace {

java_caller *retain_listener(JavaListenerState &state) {
    std::lock_guard<std::mutex> lock(state.mutex);
    return java_caller_retain(state.caller);
}

void notify_boolean(WebViewContext *ctx, JavaListenerState &state, bool value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    java_caller *caller = retain_listener(state);
    if (!caller) return;

    jvalue boxed;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        ZeroMemory(args, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

} // namespace

void emit_history_change_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview || ctx->closing.load(std::memory_order_acquire)) return;

    BOOL can_go_back = FALSE;
    BOOL can_go_forward = FALSE;
    ctx->webview->get_CanGoBack(&can_go_back);
    ctx->webview->get_CanGoForward(&can_go_forward);

    notify_boolean(ctx, ctx->can_go_back_change_listener, can_go_back == TRUE);
    notify_boolean(ctx, ctx->can_go_forward_change_listener, can_go_forward == TRUE);
}

void register_history_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview) return;

    unregister_history_events(ctx);
    ctx->webview->add_HistoryChanged(
        Callback<ICoreWebView2HistoryChangedEventHandler>(
            [ctx](ICoreWebView2 *, IUnknown *) -> HRESULT {
                emit_history_change_events(ctx);
                return S_OK;
            }
        ).Get(),
        &ctx->token_history_changed
    );
    ctx->history_changed_registered = true;
    emit_history_change_events(ctx);
}

void unregister_history_events(WebViewContext *ctx) {
    if (!ctx || !ctx->webview || !ctx->history_changed_registered) return;
    ctx->webview->remove_HistoryChanged(ctx->token_history_changed);
    ctx->history_changed_registered = false;
}

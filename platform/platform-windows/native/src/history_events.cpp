#include "history_events.h"

#include "java_listener.h"
#include "webview2_callback.h"

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

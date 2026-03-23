#include "url_events.h"

#include <cwchar>

#include "java_listener.h"
#include "webview2_callback.h"

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

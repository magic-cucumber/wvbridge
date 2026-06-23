#include "libs_helpers.h"

API_EXPORT(void, refresh, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    bool ok = true;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &ok, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->Reload();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::Reload failed (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

#include "libs_helpers.h"

API_EXPORT(jboolean, goBack, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return JNI_FALSE;

    BOOL can_go_back = FALSE;
    BOOL can_go_back_after = FALSE;
    bool ok = true;
    bool cannot_go_back = false;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_back, &can_go_back_after, &ok, &cannot_go_back, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->get_CanGoBack(&can_go_back);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_back) {
            ok = false;
            cannot_go_back = true;
            error = "webview cannot go back";
            return;
        }

        hr = ctx->webview->GoBack();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::GoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        hr = ctx->webview->get_CanGoBack(&can_go_back_after);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed after GoBack (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

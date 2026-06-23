#include "libs_helpers.h"

API_EXPORT(jboolean, goForward, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return JNI_FALSE;

    BOOL can_go_forward = FALSE;
    BOOL can_go_forward_after = FALSE;
    bool ok = true;
    bool cannot_go_forward = false;
    std::string error;
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_forward, &can_go_forward_after, &ok, &cannot_go_forward, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            ok = false;
            error = "webview is not available";
            return;
        }

        HRESULT hr = ctx->webview->get_CanGoForward(&can_go_forward);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_forward) {
            ok = false;
            cannot_go_forward = true;
            error = "webview cannot go forward";
            return;
        }

        hr = ctx->webview->GoForward();
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::GoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        hr = ctx->webview->get_CanGoForward(&can_go_forward_after);
        if (FAILED(hr)) {
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed after GoForward (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

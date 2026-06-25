#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goForward, jlong handle) {
    LOGGER_I("goForward: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("goForward: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("goForward: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("goForward: context or thread is null, returning false");
        return JNI_FALSE;
    }

    BOOL can_go_forward = FALSE;
    BOOL can_go_forward_after = FALSE;
    bool ok = true;
    bool cannot_go_forward = false;
    std::string error;
    LOGGER_V("goForward: dispatching to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_forward, &can_go_forward_after, &ok, &cannot_go_forward, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("goForward: webview not available, aborting");
            ok = false;
            error = "webview is not available";
            return;
        }

        LOGGER_V("goForward: calling get_CanGoForward");
        HRESULT hr = ctx->webview->get_CanGoForward(&can_go_forward);
        if (FAILED(hr)) {
            LOGGER_V("goForward: get_CanGoForward failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_forward) {
            LOGGER_V("goForward: webview cannot go forward");
            ok = false;
            cannot_go_forward = true;
            error = "webview cannot go forward";
            return;
        }

        LOGGER_V("goForward: calling GoForward");
        hr = ctx->webview->GoForward();
        if (FAILED(hr)) {
            LOGGER_V("goForward: GoForward failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::GoForward failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        LOGGER_V("goForward: calling get_CanGoForward after GoForward");
        hr = ctx->webview->get_CanGoForward(&can_go_forward_after);
        if (FAILED(hr)) {
            LOGGER_V("goForward: get_CanGoForward after GoForward failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::get_CanGoForward failed after GoForward (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_forward ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        LOGGER_E("goForward: operation failed, error=%s", error.c_str());
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    LOGGER_V("goForward: success, can_go_forward_after=%d", can_go_forward_after);
    return can_go_forward_after ? JNI_TRUE : JNI_FALSE;
}

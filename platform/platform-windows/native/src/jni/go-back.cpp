#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(jboolean, goBack, jlong handle) {
    LOGGER_I("goBack: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("goBack: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return JNI_FALSE;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("goBack: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("goBack: context or thread is null, returning false");
        return JNI_FALSE;
    }

    BOOL can_go_back = FALSE;
    BOOL can_go_back_after = FALSE;
    bool ok = true;
    bool cannot_go_back = false;
    std::string error;
    LOGGER_V("goBack: dispatching to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, &can_go_back, &can_go_back_after, &ok, &cannot_go_back, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("goBack: webview not available, aborting");
            ok = false;
            error = "webview is not available";
            return;
        }

        LOGGER_V("goBack: calling get_CanGoBack");
        HRESULT hr = ctx->webview->get_CanGoBack(&can_go_back);
        if (FAILED(hr)) {
            LOGGER_V("goBack: get_CanGoBack failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }
        if (!can_go_back) {
            LOGGER_V("goBack: webview cannot go back");
            ok = false;
            cannot_go_back = true;
            error = "webview cannot go back";
            return;
        }

        LOGGER_V("goBack: calling GoBack");
        hr = ctx->webview->GoBack();
        if (FAILED(hr)) {
            LOGGER_V("goBack: GoBack failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::GoBack failed (HRESULT=" + format_hresult(hr) + ")";
            return;
        }

        LOGGER_V("goBack: calling get_CanGoBack after GoBack");
        hr = ctx->webview->get_CanGoBack(&can_go_back_after);
        if (FAILED(hr)) {
            LOGGER_V("goBack: get_CanGoBack after GoBack failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::get_CanGoBack failed after GoBack (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        const char *exception = cannot_go_back ? "java/lang/IllegalStateException" : "java/lang/RuntimeException";
        LOGGER_E("goBack: operation failed, error=%s", error.c_str());
        throw_jni_exception(env, exception, error.c_str());
        return JNI_FALSE;
    }
    LOGGER_V("goBack: success, can_go_back_after=%d", can_go_back_after);
    return can_go_back_after ? JNI_TRUE : JNI_FALSE;
}

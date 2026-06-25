#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, refresh, jlong handle) {
    LOGGER_I("refresh: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("refresh: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("refresh: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("refresh: context or thread is null, aborting");
        return;
    }

    bool ok = true;
    std::string error;
    LOGGER_V("refresh: dispatching to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, &ok, &error] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) {
            LOGGER_V("refresh: webview not available, aborting");
            ok = false;
            error = "webview is not available";
            return;
        }

        LOGGER_V("refresh: calling ICoreWebView2::Reload");
        HRESULT hr = ctx->webview->Reload();
        if (FAILED(hr)) {
            LOGGER_V("refresh: Reload failed, hr=0x%lx", (unsigned long)hr);
            ok = false;
            error = "ICoreWebView2::Reload failed (HRESULT=" + format_hresult(hr) + ")";
        }
    });

    if (!ok) {
        LOGGER_E("refresh: operation failed, error=%s", error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", error.c_str());
    }
}

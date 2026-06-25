#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    LOGGER_I("loadUrl: handle=%lld url=%p", (long long)handle, url);
    if (handle == 0) {
        LOGGER_E("loadUrl: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (!url) {
        LOGGER_E("loadUrl: url is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("loadUrl: context=%p", ctx);
    if (!ctx || !ctx->thread) {
        LOGGER_W("loadUrl: context or thread is null, aborting");
        return;
    }

    const char *utf8 = env->GetStringUTFChars(url, nullptr);
    if (!utf8) {
        LOGGER_W("loadUrl: GetStringUTFChars returned null, aborting");
        return;
    }
    LOGGER_V("loadUrl: url utf8=%s", utf8);
    std::wstring wurl = utf8_to_wstring(utf8);
    env->ReleaseStringUTFChars(url, utf8);

    if (wurl.empty()) {
        LOGGER_V("loadUrl: url is empty, defaulting to about:blank");
        wurl = L"about:blank";
    }

    HRESULT hr = S_OK;
    LOGGER_V("loadUrl: dispatching Navigate to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, wurl, &hr] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_V("loadUrl: ctx closing or null, aborting");
            hr = E_FAIL;
            return;
        }
        if (!ctx->webview) {
            LOGGER_V("loadUrl: webview is null, aborting");
            hr = E_FAIL;
            return;
        }
        LOGGER_V("loadUrl: calling ICoreWebView2::Navigate");
        hr = ctx->webview->Navigate(wurl.c_str());
        LOGGER_V("loadUrl: Navigate returned hr=0x%lx", (unsigned long)hr);
    });

    if (FAILED(hr)) {
        LOGGER_E("loadUrl: Navigate failed, hr=0x%lx", (unsigned long)hr);
        std::string message = "WebView2 Navigate failed [HRESULT=" + format_hresult(hr) + "]";
        throw_jni_exception(env, "java/lang/RuntimeException", message.c_str());
    }
}

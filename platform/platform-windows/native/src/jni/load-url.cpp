#include "libs_helpers.h"

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (!url) {
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx || !ctx->thread) return;

    const char *utf8 = env->GetStringUTFChars(url, nullptr);
    if (!utf8) return;
    std::wstring wurl = utf8_to_wstring(utf8);
    env->ReleaseStringUTFChars(url, utf8);

    if (wurl.empty()) {
        wurl = L"about:blank";
    }

    HRESULT hr = S_OK;
    webview2_thread_run_sync(ctx->thread, [ctx, wurl, &hr] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            hr = E_FAIL;
            return;
        }
        if (!ctx->webview) {
            hr = E_FAIL;
            return;
        }
        hr = ctx->webview->Navigate(wurl.c_str());
    });

    if (FAILED(hr)) {
        std::string message = "WebView2 Navigate failed [HRESULT=" + format_hresult(hr) + "]";
        throw_jni_exception(env, "java/lang/RuntimeException", message.c_str());
    }
}

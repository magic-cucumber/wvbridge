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

    webview2_thread_run_sync(ctx->thread, [ctx, wurl] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire) || !ctx->webview) return;
        ctx->webview->Navigate(wurl.c_str());
    });
}

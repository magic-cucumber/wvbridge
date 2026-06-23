#include "libs_helpers.h"

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (url == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    auto *ctx = (WebViewContext *) handle;
    if (!ctx) return;

    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        // OOM 或 JVM 已抛异常
        return;
    }

    // 必须同步：否则 close0 可能先 delete ctx，导致异步闭包访问悬空指针。
    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;
        if (!ctx->webview) return;

        const char *uri = nativeString;
        if (!uri || uri[0] == '\0') uri = "about:blank";

        webkit_web_view_load_uri(ctx->webview, uri);
    });

    env->ReleaseStringUTFChars(url, nativeString);
}

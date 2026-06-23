#include "libs_helpers.h"

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        return;
    }

    NSString *result = [NSString stringWithUTF8String:nativeString];
    env->ReleaseStringUTFChars(url, nativeString);

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    runOnMainAsync(^{
        if (!ctx) return;

        [ctx->webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:result]]];
    });
}

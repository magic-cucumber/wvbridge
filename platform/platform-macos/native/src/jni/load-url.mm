#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    LOGGER_I("loadUrl: handle=%lld", (long long) handle);
    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        LOGGER_W("loadUrl: GetStringUTFChars returned null, aborting");
        return;
    }

    NSString *result = [NSString stringWithUTF8String:nativeString];
    env->ReleaseStringUTFChars(url, nativeString);
    LOGGER_V("loadUrl: url=%s", [result UTF8String]);

    if (handle == 0) {
        LOGGER_W("loadUrl: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("loadUrl: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("loadUrl: ctx is null after cast, aborting");
        return;
    }

    runOnMainAsync(^{
        LOGGER_V("loadUrl: loading on main thread, url=%s", [result UTF8String]);
        if (!ctx) return;

        [ctx->webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:result]]];
    });
}

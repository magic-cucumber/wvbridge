#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, loadUrl, jlong handle, jstring url) {
    LOGGER_I("loadUrl: handle=%lld", (long long)handle);

    if (handle == 0) {
        LOGGER_E("loadUrl: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    if (url == nullptr) {
        LOGGER_E("loadUrl: null url, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "url is null");
        return;
    }

    LOGGER_V("loadUrl: casting handle to WebViewContext");
    auto *ctx = (WebViewContext *) handle;
    if (!ctx) {
        LOGGER_W("loadUrl: ctx is null after cast, aborting");
        return;
    }

    LOGGER_V("loadUrl: getting native string from jstring");
    const char *nativeString = env->GetStringUTFChars(url, nullptr);
    if (nativeString == nullptr) {
        LOGGER_W("loadUrl: GetStringUTFChars returned null (OOM or JVM exception)");
        return;
    }
    LOGGER_V("loadUrl: nativeString=%s", nativeString);

    wvbridge::gtk_run_on_thread_sync([&] {
        if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
            LOGGER_V("loadUrl: ctx is null or closing, aborting GTK work");
            return;
        }
        if (!ctx->webview) {
            LOGGER_V("loadUrl: ctx->webview is null, aborting GTK work");
            return;
        }

        const char *uri = nativeString;
        if (!uri || uri[0] == '\0') {
            LOGGER_V("loadUrl: empty uri, falling back to about:blank");
            uri = "about:blank";
        }

        LOGGER_V("loadUrl: calling webkit_web_view_load_uri with uri=%s", uri);
        webkit_web_view_load_uri(ctx->webview, uri);
    });

    LOGGER_V("loadUrl: releasing native string");
    env->ReleaseStringUTFChars(url, nativeString);
}

#include "libs_helpers.h"

API_EXPORT(void, requestFocus0, jlong handle) {
    (void) thiz;

    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    if (!ctx) return;

    wvbridge::gtk_run_on_thread_sync([&] {
        focus_embedded_webview(ctx);
    });
}

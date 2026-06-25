#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, close0, jlong handle) {
    LOGGER_I("close0: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("close0: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }
    auto *ctx = reinterpret_cast<WebViewContext *>(handle);
    LOGGER_V("close0: context=%p", ctx);
    if (!ctx) {
        LOGGER_W("close0: context is null, nothing to destroy");
        return;
    }
    LOGGER_V("close0: destroying context");
    destroy_ctx(ctx);
}

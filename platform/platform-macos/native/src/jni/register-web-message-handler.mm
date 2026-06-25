#import "javascript-helpers.h"

#include <wvbridge/javascript.h>

API_EXPORT(jlong, registerWebMessageHandler, jlong handle, jobject callback) {
    LOGGER_I("registerWebMessageHandler: handle=%lld callback=%p", (long long) handle, callback);
    if (callback == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "callback is null");
        return 0;
    }

    auto *ctx = require_context(env, handle, "registerWebMessageHandler");
    if (!ctx) return 0;

    jlong handlerId = wvbridge::register_web_message_handler(
        env,
        ctx->webMessageHandlersMutex,
        ctx->webMessageHandlers,
        ctx->nextWebMessageHandlerId,
        callback
    );
    if (handlerId == 0) {
        throw_jni_exception(env, "java/lang/RuntimeException", "failed to retain callback");
    }
    return handlerId;
}

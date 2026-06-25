#include "javascript-helpers.h"

#include <wvbridge/javascript.h>

API_EXPORT(void, unregisterWebMessageHandler, jlong handle, jlong handlerId) {
    LOGGER_I("unregisterWebMessageHandler: handle=%lld handlerId=%lld", (long long)handle, (long long)handlerId);
    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    wvbridge::unregister_web_message_handler(
        env,
        ctx->web_message_handlers_mutex,
        ctx->web_message_handlers,
        handlerId
    );
}

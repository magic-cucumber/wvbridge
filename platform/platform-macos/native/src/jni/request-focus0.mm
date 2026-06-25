#include "libs_helpers.h"
#include <wvbridge/logger.h>

API_EXPORT(void, requestFocus0, jlong handle) {
    LOGGER_I("requestFocus0: handle=%lld (no-op stub)", (long long) handle);
    (void) env;
    (void) thiz;
    (void) handle;
}

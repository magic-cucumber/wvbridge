#include "utils.h"

#include <windows.h>

#include <wvbridge/logger.h>

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    LOGGER_I("throw_jni_exception: name=%s, message=%s", name ? name : "(null)", message ? message : "(null)");
    if (!env) {
        LOGGER_W("throw_jni_exception: null env, aborting");
        return;
    }
    if (env->ExceptionCheck()) {
        LOGGER_W("throw_jni_exception: pending JNI exception already exists, aborting");
        return;
    }

    const char *fallback = "java/lang/RuntimeException";
    const char *clazz = (name && name[0] != '\0') ? name : fallback;

    jclass cls = env->FindClass(clazz);
    if (!cls) {
        LOGGER_W("throw_jni_exception: FindClass failed for %s, trying fallback", clazz);
        if (env->ExceptionCheck()) env->ExceptionClear();
        cls = env->FindClass(fallback);
        if (!cls) {
            LOGGER_E("throw_jni_exception: FindClass failed for fallback, aborting");
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    env->ThrowNew(cls, message ? message : "");
    LOGGER_V("throw_jni_exception: ThrowNew called with class=%s", clazz);
    env->DeleteLocalRef(cls);
}

std::wstring utf8_to_wstring(const char *utf8) {
    LOGGER_V("utf8_to_wstring: utf8=%s", utf8 ? utf8 : "(null)");
    if (!utf8 || utf8[0] == '\0') {
        LOGGER_W("utf8_to_wstring: null or empty input, returning empty");
        return L"";
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (required <= 0) {
        LOGGER_W("utf8_to_wstring: MultiByteToWideChar returned %d, returning empty", required);
        return L"";
    }

    std::wstring out(static_cast<size_t>(required), L'\0');
    int converted = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), required);
    if (converted <= 0) {
        LOGGER_W("utf8_to_wstring: MultiByteToWideChar conversion returned %d, returning empty", converted);
        return L"";
    }

    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

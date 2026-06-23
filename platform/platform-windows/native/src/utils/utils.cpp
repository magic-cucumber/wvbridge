#include "utils.h"

#include <windows.h>

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    if (!env) return;
    if (env->ExceptionCheck()) return;

    const char *fallback = "java/lang/RuntimeException";
    const char *clazz = (name && name[0] != '\0') ? name : fallback;

    jclass cls = env->FindClass(clazz);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        cls = env->FindClass(fallback);
        if (!cls) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    env->ThrowNew(cls, message ? message : "");
    env->DeleteLocalRef(cls);
}

std::wstring utf8_to_wstring(const char *utf8) {
    if (!utf8 || utf8[0] == '\0') return L"";

    int required = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (required <= 0) return L"";

    std::wstring out(static_cast<size_t>(required), L'\0');
    int converted = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), required);
    if (converted <= 0) return L"";

    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

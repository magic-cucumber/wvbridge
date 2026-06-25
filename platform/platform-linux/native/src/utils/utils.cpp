//
// Created by debian on 2026/2/11.
//

#include <jni.h>
#include <string>
#include <wvbridge/logger.h>

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    LOGGER_I("throw_jni_exception: name=%s, message=%s", name ? name : "null", message ? message : "null");
    if (env == nullptr) {
        LOGGER_W("throw_jni_exception: null env, aborting");
        return;
    }

    // 若已存在 pending exception，则不要覆盖。
    if (env->ExceptionCheck()) {
        LOGGER_V("throw_jni_exception: pending exception exists, not overwriting");
        return;
    }

    const char *fallback = "java/lang/RuntimeException";

    std::string className = (name != nullptr && name[0] != '\0') ? name : fallback;
    for (auto &ch : className) {
        if (ch == '.') ch = '/';
    }

    LOGGER_V("throw_jni_exception: finding class=%s", className.c_str());
    jclass excClass = env->FindClass(className.c_str());
    if (excClass == nullptr) {
        LOGGER_V("throw_jni_exception: FindClass failed for %s, falling back to %s", className.c_str(), fallback);
        // FindClass 失败会留下 ClassNotFoundException；这里清掉并回退到 RuntimeException。
        if (env->ExceptionCheck()) env->ExceptionClear();
        excClass = env->FindClass(fallback);
        if (excClass == nullptr) {
            LOGGER_W("throw_jni_exception: FindClass failed for fallback, aborting");
            // 连 RuntimeException 都找不到：避免留下非预期异常。
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    const char *msg = (message != nullptr) ? message : "";
    LOGGER_V("throw_jni_exception: throwing exception class=%s, msg=%s", className.c_str(), msg);
    env->ThrowNew(excClass, msg);
    env->DeleteLocalRef(excClass);
}

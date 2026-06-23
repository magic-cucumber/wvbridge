//
// Created by debian on 2026/2/11.
//

#include <jni.h>
#include <string>

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    if (env == nullptr) return;

    // 若已存在 pending exception，则不要覆盖。
    if (env->ExceptionCheck()) return;

    const char *fallback = "java/lang/RuntimeException";

    std::string className = (name != nullptr && name[0] != '\0') ? name : fallback;
    for (auto &ch : className) {
        if (ch == '.') ch = '/';
    }

    jclass excClass = env->FindClass(className.c_str());
    if (excClass == nullptr) {
        // FindClass 失败会留下 ClassNotFoundException；这里清掉并回退到 RuntimeException。
        if (env->ExceptionCheck()) env->ExceptionClear();
        excClass = env->FindClass(fallback);
        if (excClass == nullptr) {
            // 连 RuntimeException 都找不到：避免留下非预期异常。
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
    }

    const char *msg = (message != nullptr) ? message : "";
    env->ThrowNew(excClass, msg);
    env->DeleteLocalRef(excClass);
}

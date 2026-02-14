#include "utils.h"

#include <string>

void throw_jni_exception(JNIEnv *env, const char *name, const char *message) {
    if (!env) return;
    if (env->ExceptionCheck()) return;

    jclass cls = env->FindClass(name);
    if (!cls) {
        // FindClass 失败时 JVM 会设置 pending exception（ClassNotFound）
        return;
    }

    env->ThrowNew(cls, message ? message : "");
    env->DeleteLocalRef(cls);
}

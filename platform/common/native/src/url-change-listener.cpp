#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmStaticCallback g_url_change_callback;
}

void notify_url_change_to_jvm(jlong pointer, wvbridge_native_string url) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_url_change_callback,
        "onURLChangeCallback",
        "(JLjava/lang/String;)V",
        &callback_class
    );
    if (method != nullptr && callback_class != nullptr) {
        jstring value = new_jvm_string(env, url);
        if (value != nullptr) {
            env->CallStaticVoidMethod(callback_class, method, pointer, value);
            clear_jni_exception(env);
            env->DeleteLocalRef(value);
        }
    }
    java_runtime_detach_env(attached);
}

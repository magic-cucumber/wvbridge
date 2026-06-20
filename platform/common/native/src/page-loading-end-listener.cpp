#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmStaticCallback g_page_loading_end_callback;
}

void notify_page_loading_end_to_jvm(
    jlong pointer,
    jboolean success,
    wvbridge_native_string reason
) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_page_loading_end_callback,
        "onPageLoadingEndCallback",
        "(JZLjava/lang/String;)V",
        &callback_class
    );
    if (method != nullptr && callback_class != nullptr) {
        jstring value = reason != nullptr ? new_jvm_string(env, reason) : nullptr;
        env->CallStaticVoidMethod(callback_class, method, pointer, success, value);
        clear_jni_exception(env);
        if (value != nullptr) env->DeleteLocalRef(value);
    }
    java_runtime_detach_env(attached);
}

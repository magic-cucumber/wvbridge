#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmStaticCallback g_can_go_forward_change_callback;
}

void notify_can_go_forward_change_to_jvm(jlong pointer, jboolean can_go_forward) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_can_go_forward_change_callback,
        "onCanGoForwardChangeCallback",
        "(JZ)V",
        &callback_class
    );
    if (method != nullptr && callback_class != nullptr) {
        env->CallStaticVoidMethod(callback_class, method, pointer, can_go_forward);
        clear_jni_exception(env);
    }
    java_runtime_detach_env(attached);
}

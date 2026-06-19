#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmListener g_can_go_back_change_listener;
}

extern "C" JNIEXPORT void JNICALL
Java_top_kagg886_wvbridge_internal_listener_NativeBridge_setCanGoBackChangeListener(
    JNIEnv* env,
    jobject,
    jobject listener
) {
    set_jvm_listener(env, g_can_go_back_change_listener, listener, "onCanGoBackChange", "(JZ)V");
}

void notify_can_go_back_change_to_jvm(jlong pointer, jboolean can_go_back) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jmethodID method = nullptr;
    jobject listener = acquire_jvm_listener(env, g_can_go_back_change_listener, &method);
    if (listener != nullptr) {
        env->CallVoidMethod(listener, method, pointer, can_go_back);
        clear_jni_exception(env);
        env->DeleteLocalRef(listener);
    }
    java_runtime_detach_env(attached);
}

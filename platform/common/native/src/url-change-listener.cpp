#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmListener g_url_change_listener;
}

extern "C" JNIEXPORT void JNICALL
Java_top_kagg886_wvbridge_internal_listener_NativeBridge_setURLChangeListener(
    JNIEnv* env,
    jobject,
    jobject listener
) {
    set_jvm_listener(env, g_url_change_listener, listener, "onURLChange", "(JLjava/lang/String;)V");
}

void notify_url_change_to_jvm(jlong pointer, wvbridge_native_string url) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jmethodID method = nullptr;
    jobject listener = acquire_jvm_listener(env, g_url_change_listener, &method);
    if (listener != nullptr) {
        jstring value = new_jvm_string(env, url);
        if (value != nullptr) {
            env->CallVoidMethod(listener, method, pointer, value);
            clear_jni_exception(env);
            env->DeleteLocalRef(value);
        }
        env->DeleteLocalRef(listener);
    }
    java_runtime_detach_env(attached);
}

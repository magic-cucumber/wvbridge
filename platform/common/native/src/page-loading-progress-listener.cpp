#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmListener g_page_loading_progress_listener;
}

extern "C" JNIEXPORT void JNICALL
Java_top_kagg886_wvbridge_internal_listener_NativeBridge_setPageLoadingProgressListener(
    JNIEnv* env,
    jobject,
    jobject listener
) {
    set_jvm_listener(
        env,
        g_page_loading_progress_listener,
        listener,
        "onPageLoadingProgress",
        "(JF)V"
    );
}

void notify_page_loading_progress_to_jvm(jlong pointer, jfloat progress) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jmethodID method = nullptr;
    jobject listener = acquire_jvm_listener(env, g_page_loading_progress_listener, &method);
    if (listener != nullptr) {
        env->CallVoidMethod(listener, method, pointer, progress);
        clear_jni_exception(env);
        env->DeleteLocalRef(listener);
    }
    java_runtime_detach_env(attached);
}

#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"

namespace {
JvmStaticCallback g_page_loading_progress_callback;
}

void notify_page_loading_progress_to_jvm(jlong pointer, jfloat progress) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_page_loading_progress_callback,
        "onPageLoadingProgressCallback",
        "(JF)V",
        &callback_class
    );
    if (method != nullptr && callback_class != nullptr) {
        env->CallStaticVoidMethod(callback_class, method, pointer, progress);
        clear_jni_exception(env);
    }
    java_runtime_detach_env(attached);
}

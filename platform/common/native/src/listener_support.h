#pragma once

#include <jni.h>

#include <mutex>

struct JvmStaticCallback {
    std::mutex mutex;
    jmethodID method = nullptr;
};

extern "C" void listener_support_on_load(JNIEnv* env);

jmethodID acquire_native_bridge_callback(
    JNIEnv* env,
    JvmStaticCallback& callback,
    const char* method_name,
    const char* signature,
    jclass* callback_class
);

#if defined(_WIN32)
jstring new_jvm_string(JNIEnv* env, const wchar_t* value);
#else
jstring new_jvm_string(JNIEnv* env, const char* value);
#endif

void clear_jni_exception(JNIEnv* env);

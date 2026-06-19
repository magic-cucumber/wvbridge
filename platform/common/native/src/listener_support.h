#pragma once

#include <jni.h>

#include <mutex>

struct JvmListener {
    std::mutex mutex;
    jobject callback = nullptr;
    jmethodID method = nullptr;
};

void set_jvm_listener(
    JNIEnv* env,
    JvmListener& listener,
    jobject callback,
    const char* method_name,
    const char* signature
);

jobject acquire_jvm_listener(JNIEnv* env, JvmListener& listener, jmethodID* method);

#if defined(_WIN32)
jstring new_jvm_string(JNIEnv* env, const wchar_t* value);
#else
jstring new_jvm_string(JNIEnv* env, const char* value);
#endif

void clear_jni_exception(JNIEnv* env);

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved);

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved);

JavaVM* java_runtime_get_vm(void);

JNIEnv* java_runtime_get_env(int* attached);

void java_runtime_detach_env(int attached);

#ifdef __cplusplus
}
#endif

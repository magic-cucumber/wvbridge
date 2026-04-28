#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct java_caller java_caller;

typedef enum java_caller_status {
    JAVA_CALLER_OK = 0,
    JAVA_CALLER_ERR_INVALID_ARGUMENT,
    JAVA_CALLER_ERR_JVM_NOT_LOADED,
    JAVA_CALLER_ERR_ATTACH_FAILED,
    JAVA_CALLER_ERR_CLASS_NOT_FOUND,
    JAVA_CALLER_ERR_METHOD_NOT_FOUND,
    JAVA_CALLER_ERR_GLOBAL_REF_FAILED,
    JAVA_CALLER_ERR_INVALID_SIGNATURE,
    JAVA_CALLER_ERR_JNI_EXCEPTION
} java_caller_status;

java_caller_status java_caller_create(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        java_caller** out_caller
);

java_caller_status java_caller_invoke(
        java_caller* caller,
        const jvalue* args,
        jvalue* result
);

java_caller* java_caller_retain(java_caller* caller);

void java_caller_release(java_caller* caller);

void java_caller_delete_global_ref(jobject object);

void java_caller_destroy(java_caller* caller);

#ifdef __cplusplus
}
#endif

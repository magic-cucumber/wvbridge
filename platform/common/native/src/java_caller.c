#include "wvbridge/java_caller.h"

#include "wvbridge/java_runtime.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef enum java_caller_return_kind {
    JAVA_CALLER_KIND_VOID,
    JAVA_CALLER_KIND_OBJECT,
    JAVA_CALLER_KIND_BOOLEAN,
    JAVA_CALLER_KIND_BYTE,
    JAVA_CALLER_KIND_CHAR,
    JAVA_CALLER_KIND_SHORT,
    JAVA_CALLER_KIND_INT,
    JAVA_CALLER_KIND_LONG,
    JAVA_CALLER_KIND_FLOAT,
    JAVA_CALLER_KIND_DOUBLE
} java_caller_return_kind;

struct java_caller {
    jobject object;
    jmethodID method;
    java_caller_return_kind return_kind;
    int destroying;
    int active_calls;
    int ref_count;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
    CONDITION_VARIABLE idle;
#else
    pthread_mutex_t mutex;
    pthread_cond_t idle;
#endif
};

static void lock_caller(java_caller* caller) {
#ifdef _WIN32
    EnterCriticalSection(&caller->mutex);
#else
    pthread_mutex_lock(&caller->mutex);
#endif
}

static void unlock_caller(java_caller* caller) {
#ifdef _WIN32
    LeaveCriticalSection(&caller->mutex);
#else
    pthread_mutex_unlock(&caller->mutex);
#endif
}

static void wait_for_idle(java_caller* caller) {
#ifdef _WIN32
    while (caller->active_calls > 0 || caller->ref_count > 1) {
        SleepConditionVariableCS(&caller->idle, &caller->mutex, INFINITE);
    }
#else
    while (caller->active_calls > 0 || caller->ref_count > 1) {
        pthread_cond_wait(&caller->idle, &caller->mutex);
    }
#endif
}

static void signal_idle(java_caller* caller) {
#ifdef _WIN32
    WakeAllConditionVariable(&caller->idle);
#else
    pthread_cond_broadcast(&caller->idle);
#endif
}

static int init_lock(java_caller* caller) {
#ifdef _WIN32
    InitializeCriticalSection(&caller->mutex);
    InitializeConditionVariable(&caller->idle);
    return 1;
#else
    if (pthread_mutex_init(&caller->mutex, NULL) != 0) {
        return 0;
    }
    if (pthread_cond_init(&caller->idle, NULL) != 0) {
        pthread_mutex_destroy(&caller->mutex);
        return 0;
    }
    return 1;
#endif
}

static void destroy_lock(java_caller* caller) {
#ifdef _WIN32
    DeleteCriticalSection(&caller->mutex);
#else
    pthread_cond_destroy(&caller->idle);
    pthread_mutex_destroy(&caller->mutex);
#endif
}

static java_caller_status parse_return_kind(const char* signature, java_caller_return_kind* out_kind) {
    if (signature == NULL || out_kind == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    const char* close = strchr(signature, ')');
    if (close == NULL || close[1] == '\0') {
        return JAVA_CALLER_ERR_INVALID_SIGNATURE;
    }

    switch (close[1]) {
        case 'V':
            *out_kind = JAVA_CALLER_KIND_VOID;
            return JAVA_CALLER_OK;
        case 'Z':
            *out_kind = JAVA_CALLER_KIND_BOOLEAN;
            return JAVA_CALLER_OK;
        case 'B':
            *out_kind = JAVA_CALLER_KIND_BYTE;
            return JAVA_CALLER_OK;
        case 'C':
            *out_kind = JAVA_CALLER_KIND_CHAR;
            return JAVA_CALLER_OK;
        case 'S':
            *out_kind = JAVA_CALLER_KIND_SHORT;
            return JAVA_CALLER_OK;
        case 'I':
            *out_kind = JAVA_CALLER_KIND_INT;
            return JAVA_CALLER_OK;
        case 'J':
            *out_kind = JAVA_CALLER_KIND_LONG;
            return JAVA_CALLER_OK;
        case 'F':
            *out_kind = JAVA_CALLER_KIND_FLOAT;
            return JAVA_CALLER_OK;
        case 'D':
            *out_kind = JAVA_CALLER_KIND_DOUBLE;
            return JAVA_CALLER_OK;
        case 'L':
            if (strchr(close + 2, ';') == NULL) {
                return JAVA_CALLER_ERR_INVALID_SIGNATURE;
            }
            *out_kind = JAVA_CALLER_KIND_OBJECT;
            return JAVA_CALLER_OK;
        case '[':
            *out_kind = JAVA_CALLER_KIND_OBJECT;
            return JAVA_CALLER_OK;
        default:
            return JAVA_CALLER_ERR_INVALID_SIGNATURE;
    }
}

static void clear_pending_exception(JNIEnv* env) {
    if (env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

java_caller_status java_caller_create(
        JNIEnv* env,
        jobject object,
        const char* name,
        const char* signature,
        java_caller** out_caller
) {
    if (out_caller != NULL) {
        *out_caller = NULL;
    }
    if (env == NULL || object == NULL || name == NULL || signature == NULL || out_caller == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    java_caller_return_kind return_kind;
    java_caller_status status = parse_return_kind(signature, &return_kind);
    if (status != JAVA_CALLER_OK) {
        return status;
    }

    jclass cls = (*env)->GetObjectClass(env, object);
    if (cls == NULL) {
        clear_pending_exception(env);
        return JAVA_CALLER_ERR_CLASS_NOT_FOUND;
    }

    jmethodID method = (*env)->GetMethodID(env, cls, name, signature);
    (*env)->DeleteLocalRef(env, cls);
    if (method == NULL) {
        clear_pending_exception(env);
        return JAVA_CALLER_ERR_METHOD_NOT_FOUND;
    }

    jobject global = (*env)->NewGlobalRef(env, object);
    if (global == NULL) {
        clear_pending_exception(env);
        return JAVA_CALLER_ERR_GLOBAL_REF_FAILED;
    }

    java_caller* caller = (java_caller*) calloc(1, sizeof(java_caller));
    if (caller == NULL) {
        (*env)->DeleteGlobalRef(env, global);
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }
    if (!init_lock(caller)) {
        (*env)->DeleteGlobalRef(env, global);
        free(caller);
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    caller->object = global;
    caller->method = method;
    caller->return_kind = return_kind;
    caller->ref_count = 1;
    *out_caller = caller;
    return JAVA_CALLER_OK;
}

java_caller_status java_caller_invoke(
        java_caller* caller,
        const jvalue* args,
        jvalue* result
) {
    if (caller == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == NULL) {
        return java_runtime_get_vm() == NULL ? JAVA_CALLER_ERR_JVM_NOT_LOADED : JAVA_CALLER_ERR_ATTACH_FAILED;
    }

    lock_caller(caller);
    if (caller->destroying || caller->object == NULL || caller->method == NULL) {
        unlock_caller(caller);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    jobject object = (*env)->NewGlobalRef(env, caller->object);
    if (object == NULL) {
        clear_pending_exception(env);
        unlock_caller(caller);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_GLOBAL_REF_FAILED;
    }

    caller->active_calls++;
    const jmethodID method = caller->method;
    const java_caller_return_kind return_kind = caller->return_kind;
    unlock_caller(caller);

    jvalue value;
    memset(&value, 0, sizeof(value));

    switch (return_kind) {
        case JAVA_CALLER_KIND_VOID:
            (*env)->CallVoidMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_OBJECT:
            value.l = (*env)->CallObjectMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_BOOLEAN:
            value.z = (*env)->CallBooleanMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_BYTE:
            value.b = (*env)->CallByteMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_CHAR:
            value.c = (*env)->CallCharMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_SHORT:
            value.s = (*env)->CallShortMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_INT:
            value.i = (*env)->CallIntMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_LONG:
            value.j = (*env)->CallLongMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_FLOAT:
            value.f = (*env)->CallFloatMethodA(env, object, method, args);
            break;
        case JAVA_CALLER_KIND_DOUBLE:
            value.d = (*env)->CallDoubleMethodA(env, object, method, args);
            break;
    }

    java_caller_status status = JAVA_CALLER_OK;
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        status = JAVA_CALLER_ERR_JNI_EXCEPTION;
    } else if (return_kind == JAVA_CALLER_KIND_OBJECT) {
        if (result != NULL) {
            memset(result, 0, sizeof(*result));
            if (value.l != NULL) {
                result->l = (*env)->NewGlobalRef(env, value.l);
                if (result->l == NULL) {
                    clear_pending_exception(env);
                    status = JAVA_CALLER_ERR_GLOBAL_REF_FAILED;
                }
            }
        }
        if (value.l != NULL) {
            (*env)->DeleteLocalRef(env, value.l);
        }
    } else if (result != NULL) {
        *result = value;
    }

    (*env)->DeleteGlobalRef(env, object);

    lock_caller(caller);
    caller->active_calls--;
    if (caller->destroying && caller->active_calls == 0) {
        signal_idle(caller);
    }
    unlock_caller(caller);

    java_runtime_detach_env(attached);
    return status;
}

java_caller* java_caller_retain(java_caller* caller) {
    if (caller == NULL) {
        return NULL;
    }

    lock_caller(caller);
    if (caller->destroying) {
        unlock_caller(caller);
        return NULL;
    }
    caller->ref_count++;
    unlock_caller(caller);
    return caller;
}

void java_caller_release(java_caller* caller) {
    if (caller == NULL) {
        return;
    }

    lock_caller(caller);
    if (caller->ref_count > 1) {
        caller->ref_count--;
    }
    if (caller->destroying && caller->ref_count == 1 && caller->active_calls == 0) {
        signal_idle(caller);
    }
    unlock_caller(caller);
}

void java_caller_delete_global_ref(jobject object) {
    if (object == NULL) {
        return;
    }

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env != NULL) {
        (*env)->DeleteGlobalRef(env, object);
    }
    java_runtime_detach_env(attached);
}

void java_caller_destroy(java_caller* caller) {
    if (caller == NULL) {
        return;
    }

    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);

    lock_caller(caller);
    caller->destroying = 1;
    wait_for_idle(caller);

    jobject object = caller->object;
    caller->object = NULL;
    caller->method = NULL;
    unlock_caller(caller);

    if (env != NULL && object != NULL) {
        (*env)->DeleteGlobalRef(env, object);
    }
    java_runtime_detach_env(attached);

    destroy_lock(caller);
    free(caller);
}

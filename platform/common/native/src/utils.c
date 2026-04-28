#include "wvbridge/utils.h"

#include "wvbridge/java_runtime.h"

#include <string.h>

static void clear_pending_exception(JNIEnv* env) {
    if (env != NULL && (*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }
}

static java_caller_status get_runtime_env(JNIEnv** out_env, int* attached) {
    if (out_env == NULL || attached == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    *out_env = java_runtime_get_env(attached);
    if (*out_env == NULL) {
        return java_runtime_get_vm() == NULL ? JAVA_CALLER_ERR_JVM_NOT_LOADED : JAVA_CALLER_ERR_ATTACH_FAILED;
    }
    return JAVA_CALLER_OK;
}

static java_caller_status pack_static_value_of(
        const char* class_name,
        const char* signature,
        jvalue arg,
        jvalue* out_value
) {
    if (class_name == NULL || signature == NULL || out_value == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    memset(out_value, 0, sizeof(*out_value));

    int attached = 0;
    JNIEnv* env = NULL;
    java_caller_status status = get_runtime_env(&env, &attached);
    if (status != JAVA_CALLER_OK) {
        return status;
    }

    jclass cls = (*env)->FindClass(env, class_name);
    if (cls == NULL) {
        clear_pending_exception(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_CLASS_NOT_FOUND;
    }

    jmethodID value_of = (*env)->GetStaticMethodID(env, cls, "valueOf", signature);
    if (value_of == NULL) {
        clear_pending_exception(env);
        (*env)->DeleteLocalRef(env, cls);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_METHOD_NOT_FOUND;
    }

    jobject local = (*env)->CallStaticObjectMethodA(env, cls, value_of, &arg);
    (*env)->DeleteLocalRef(env, cls);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_JNI_EXCEPTION;
    }
    if (local == NULL) {
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_GLOBAL_REF_FAILED;
    }

    out_value->l = (*env)->NewGlobalRef(env, local);
    (*env)->DeleteLocalRef(env, local);
    if (out_value->l == NULL) {
        clear_pending_exception(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_GLOBAL_REF_FAILED;
    }

    java_runtime_detach_env(attached);
    return JAVA_CALLER_OK;
}

static java_caller_status depack_primitive(
        jvalue value,
        const char* method_name,
        const char* signature,
        jvalue* out_value,
        char result_kind
) {
    if (value.l == NULL || method_name == NULL || signature == NULL || out_value == NULL) {
        return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    memset(out_value, 0, sizeof(*out_value));

    int attached = 0;
    JNIEnv* env = NULL;
    java_caller_status status = get_runtime_env(&env, &attached);
    if (status != JAVA_CALLER_OK) {
        return status;
    }

    jclass cls = (*env)->GetObjectClass(env, value.l);
    if (cls == NULL) {
        clear_pending_exception(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_CLASS_NOT_FOUND;
    }

    jmethodID method = (*env)->GetMethodID(env, cls, method_name, signature);
    (*env)->DeleteLocalRef(env, cls);
    if (method == NULL) {
        clear_pending_exception(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_METHOD_NOT_FOUND;
    }

    switch (result_kind) {
        case 'Z':
            out_value->z = (*env)->CallBooleanMethodA(env, value.l, method, NULL);
            break;
        case 'B':
            out_value->b = (*env)->CallByteMethodA(env, value.l, method, NULL);
            break;
        case 'C':
            out_value->c = (*env)->CallCharMethodA(env, value.l, method, NULL);
            break;
        case 'S':
            out_value->s = (*env)->CallShortMethodA(env, value.l, method, NULL);
            break;
        case 'I':
            out_value->i = (*env)->CallIntMethodA(env, value.l, method, NULL);
            break;
        case 'J':
            out_value->j = (*env)->CallLongMethodA(env, value.l, method, NULL);
            break;
        case 'F':
            out_value->f = (*env)->CallFloatMethodA(env, value.l, method, NULL);
            break;
        case 'D':
            out_value->d = (*env)->CallDoubleMethodA(env, value.l, method, NULL);
            break;
        default:
            java_runtime_detach_env(attached);
            return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    }

    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        java_runtime_detach_env(attached);
        return JAVA_CALLER_ERR_JNI_EXCEPTION;
    }

    java_runtime_detach_env(attached);
    return JAVA_CALLER_OK;
}

java_caller_status java_caller_pack_boolean(jboolean value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.z = value;
    return pack_static_value_of("java/lang/Boolean", "(Z)Ljava/lang/Boolean;", arg, out_value);
}

java_caller_status java_caller_pack_byte(jbyte value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.b = value;
    return pack_static_value_of("java/lang/Byte", "(B)Ljava/lang/Byte;", arg, out_value);
}

java_caller_status java_caller_pack_char(jchar value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.c = value;
    return pack_static_value_of("java/lang/Character", "(C)Ljava/lang/Character;", arg, out_value);
}

java_caller_status java_caller_pack_short(jshort value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.s = value;
    return pack_static_value_of("java/lang/Short", "(S)Ljava/lang/Short;", arg, out_value);
}

java_caller_status java_caller_pack_int(jint value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.i = value;
    return pack_static_value_of("java/lang/Integer", "(I)Ljava/lang/Integer;", arg, out_value);
}

java_caller_status java_caller_pack_long(jlong value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.j = value;
    return pack_static_value_of("java/lang/Long", "(J)Ljava/lang/Long;", arg, out_value);
}

java_caller_status java_caller_pack_float(jfloat value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.f = value;
    return pack_static_value_of("java/lang/Float", "(F)Ljava/lang/Float;", arg, out_value);
}

java_caller_status java_caller_pack_double(jdouble value, jvalue* out_value) {
    jvalue arg;
    memset(&arg, 0, sizeof(arg));
    arg.d = value;
    return pack_static_value_of("java/lang/Double", "(D)Ljava/lang/Double;", arg, out_value);
}

java_caller_status java_caller_depack_boolean(jvalue value, jboolean* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "booleanValue", "()Z", &result, 'Z');
    if (status == JAVA_CALLER_OK) *out_value = result.z;
    return status;
}

java_caller_status java_caller_depack_byte(jvalue value, jbyte* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "byteValue", "()B", &result, 'B');
    if (status == JAVA_CALLER_OK) *out_value = result.b;
    return status;
}

java_caller_status java_caller_depack_char(jvalue value, jchar* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "charValue", "()C", &result, 'C');
    if (status == JAVA_CALLER_OK) *out_value = result.c;
    return status;
}

java_caller_status java_caller_depack_short(jvalue value, jshort* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "shortValue", "()S", &result, 'S');
    if (status == JAVA_CALLER_OK) *out_value = result.s;
    return status;
}

java_caller_status java_caller_depack_int(jvalue value, jint* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "intValue", "()I", &result, 'I');
    if (status == JAVA_CALLER_OK) *out_value = result.i;
    return status;
}

java_caller_status java_caller_depack_long(jvalue value, jlong* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "longValue", "()J", &result, 'J');
    if (status == JAVA_CALLER_OK) *out_value = result.j;
    return status;
}

java_caller_status java_caller_depack_float(jvalue value, jfloat* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "floatValue", "()F", &result, 'F');
    if (status == JAVA_CALLER_OK) *out_value = result.f;
    return status;
}

java_caller_status java_caller_depack_double(jvalue value, jdouble* out_value) {
    if (out_value == NULL) return JAVA_CALLER_ERR_INVALID_ARGUMENT;
    jvalue result;
    java_caller_status status = depack_primitive(value, "doubleValue", "()D", &result, 'D');
    if (status == JAVA_CALLER_OK) *out_value = result.d;
    return status;
}

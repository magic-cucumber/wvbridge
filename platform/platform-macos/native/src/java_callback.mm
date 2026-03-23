#import "java_callback.h"

namespace {

JNIEnv *attach_env(JavaVM *jvm, bool *didAttach) {
    if (didAttach) *didAttach = false;
    if (jvm == nullptr) return nullptr;

    JNIEnv *env = nullptr;
    const jint status = jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status == JNI_OK) return env;
    if (status == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread((void **) &env, nullptr) != JNI_OK) {
            return nullptr;
        }
        if (didAttach) *didAttach = true;
        return env;
    }
    return nullptr;
}

void detach_env(JavaVM *jvm, bool didAttach) {
    if (jvm != nullptr && didAttach) {
        jvm->DetachCurrentThread();
    }
}

void call_callback(JNIEnv *env, JavaCallbackState *state, jobject arg) {
    if (env == nullptr || state == nullptr || state->listener == nullptr || state->method == nullptr || arg == nullptr) {
        return;
    }

    if (state->useAccept) {
        env->CallVoidMethod(state->listener, state->method, arg);
    } else {
        jobject result = env->CallObjectMethod(state->listener, state->method, arg);
        if (result != nullptr) {
            env->DeleteLocalRef(result);
        }
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

} // namespace

void java_callback_init(JavaCallbackState *state, JavaVM *jvm) {
    if (state == nullptr) return;
    state->jvm = jvm;
    state->listener = nullptr;
    state->method = nullptr;
    state->useAccept = YES;
}

void java_callback_set(JNIEnv *env, JavaCallbackState *state, jobject listener) {
    if (env == nullptr || state == nullptr) return;

    if (state->listener != nullptr) {
        env->DeleteGlobalRef(state->listener);
        state->listener = nullptr;
        state->method = nullptr;
    }

    if (listener == nullptr) return;

    jobject global = env->NewGlobalRef(listener);
    if (global == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jclass cls = env->GetObjectClass(listener);
    if (cls == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    jmethodID method = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;)V");
    BOOL useAccept = YES;
    if (method == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        method = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;");
        useAccept = NO;
    }
    env->DeleteLocalRef(cls);

    if (method == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    state->listener = global;
    state->method = method;
    state->useAccept = useAccept;
}

void java_callback_dispose(JavaCallbackState *state) {
    if (state == nullptr || state->jvm == nullptr || state->listener == nullptr) return;

    bool didAttach = false;
    JNIEnv *env = attach_env(state->jvm, &didAttach);
    if (env != nullptr) {
        env->DeleteGlobalRef(state->listener);
    }
    state->listener = nullptr;
    state->method = nullptr;
    detach_env(state->jvm, didAttach);
}

void java_callback_call_string(JavaCallbackState *state, NSString *value) {
    if (state == nullptr || state->jvm == nullptr || state->listener == nullptr || state->method == nullptr) return;

    bool didAttach = false;
    JNIEnv *env = attach_env(state->jvm, &didAttach);
    if (env == nullptr) {
        detach_env(state->jvm, didAttach);
        return;
    }

    const char *utf8 = (value ?: @"").UTF8String;
    jstring boxed = env->NewStringUTF(utf8 ? utf8 : "");
    if (boxed != nullptr) {
        call_callback(env, state, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(state->jvm, didAttach);
}

void java_callback_call_float(JavaCallbackState *state, float value) {
    if (state == nullptr || state->jvm == nullptr || state->listener == nullptr || state->method == nullptr) return;

    bool didAttach = false;
    JNIEnv *env = attach_env(state->jvm, &didAttach);
    if (env == nullptr) {
        detach_env(state->jvm, didAttach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Float");
    if (cls == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(state->jvm, didAttach);
        return;
    }

    jmethodID valueOf = env->GetStaticMethodID(cls, "valueOf", "(F)Ljava/lang/Float;");
    if (valueOf == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(state->jvm, didAttach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, valueOf, value);
    env->DeleteLocalRef(cls);
    if (boxed != nullptr && !env->ExceptionCheck()) {
        call_callback(env, state, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(state->jvm, didAttach);
}

void java_callback_call_boolean(JavaCallbackState *state, BOOL value) {
    if (state == nullptr || state->jvm == nullptr || state->listener == nullptr || state->method == nullptr) return;

    bool didAttach = false;
    JNIEnv *env = attach_env(state->jvm, &didAttach);
    if (env == nullptr) {
        detach_env(state->jvm, didAttach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Boolean");
    if (cls == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(state->jvm, didAttach);
        return;
    }

    jmethodID valueOf = env->GetStaticMethodID(cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    if (valueOf == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(state->jvm, didAttach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, valueOf, value ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(cls);
    if (boxed != nullptr && !env->ExceptionCheck()) {
        call_callback(env, state, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(state->jvm, didAttach);
}

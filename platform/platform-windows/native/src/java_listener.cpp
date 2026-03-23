#include "java_listener.h"

#include <cwchar>

namespace {

void invoke_listener(JNIEnv *env, JavaListenerState &state, jobject arg1, jobject arg2) {
    if (!env) return;

    jobject callback = nullptr;
    jmethodID mid = nullptr;
    bool use_accept = true;
    bool two_args = false;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        if (state.global && state.mid) {
            callback = env->NewGlobalRef(state.global);
            mid = state.mid;
            use_accept = state.use_accept;
            two_args = state.two_args;
        }
    }

    if (!callback || !mid) return;

    if (two_args) {
        if (use_accept) {
            env->CallVoidMethod(callback, mid, arg1, arg2);
        } else {
            jobject result = env->CallObjectMethod(callback, mid, arg1, arg2);
            if (result) env->DeleteLocalRef(result);
        }
    } else {
        if (!arg1) {
            env->DeleteGlobalRef(callback);
            return;
        }
        if (use_accept) {
            env->CallVoidMethod(callback, mid, arg1);
        } else {
            jobject result = env->CallObjectMethod(callback, mid, arg1);
            if (result) env->DeleteLocalRef(result);
        }
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteGlobalRef(callback);
}

} // namespace

JNIEnv *attach_env(JavaVM *jvm, bool *did_attach) {
    if (did_attach) *did_attach = false;
    if (!jvm) return nullptr;

    JNIEnv *env = nullptr;
    jint result = jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
    if (result == JNI_OK) return env;
    if (result == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
        if (did_attach) *did_attach = true;
        return env;
    }
    return nullptr;
}

void detach_env(JavaVM *jvm, bool did_attach) {
    if (jvm && did_attach) {
        jvm->DetachCurrentThread();
    }
}

void replace_listener(JNIEnv *env, JavaListenerState &state, jobject listener) {
    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.global) {
        env->DeleteGlobalRef(state.global);
        state.global = nullptr;
        state.mid = nullptr;
        state.two_args = false;
    }

    if (!listener) return;

    jobject global = env->NewGlobalRef(listener);
    if (!global) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jclass cls = env->GetObjectClass(listener);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    jmethodID mid = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;)V");
    bool use_accept = true;
    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        mid = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;");
        use_accept = false;
    }
    env->DeleteLocalRef(cls);

    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    state.global = global;
    state.mid = mid;
    state.use_accept = use_accept;
    state.two_args = false;
}

void replace_listener_with_two_args(JNIEnv *env, JavaListenerState &state, jobject listener) {
    std::lock_guard<std::mutex> lock(state.mutex);

    if (state.global) {
        env->DeleteGlobalRef(state.global);
        state.global = nullptr;
        state.mid = nullptr;
        state.two_args = false;
    }

    if (!listener) return;

    jobject global = env->NewGlobalRef(listener);
    if (!global) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jclass cls = env->GetObjectClass(listener);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    jmethodID mid = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    bool use_accept = true;
    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        mid = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        use_accept = false;
    }
    env->DeleteLocalRef(cls);

    if (!mid) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    state.global = global;
    state.mid = mid;
    state.use_accept = use_accept;
    state.two_args = true;
}

void clear_listener(JNIEnv *env, JavaListenerState &state) {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.global) return;

    env->DeleteGlobalRef(state.global);
    state.global = nullptr;
    state.mid = nullptr;
    state.two_args = false;
}

void notify_string(WebViewContext *ctx, JavaListenerState &state, const wchar_t *value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    bool did_attach = false;
    JNIEnv *env = attach_env(ctx->jvm, &did_attach);
    if (!env) {
        detach_env(ctx->jvm, did_attach);
        return;
    }

    const wchar_t *safe_value = value ? value : L"";
    const jchar *chars = reinterpret_cast<const jchar *>(safe_value);
    const jsize len = static_cast<jsize>(wcslen(safe_value));
    jstring boxed = env->NewString(chars, len);
    if (boxed) {
        invoke_listener(env, state, boxed, nullptr);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(ctx->jvm, did_attach);
}

void notify_float(WebViewContext *ctx, JavaListenerState &state, float value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    bool did_attach = false;
    JNIEnv *env = attach_env(ctx->jvm, &did_attach);
    if (!env) {
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Float");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jmethodID value_of = env->GetStaticMethodID(cls, "valueOf", "(F)Ljava/lang/Float;");
    if (!value_of) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, value_of, value);
    env->DeleteLocalRef(cls);
    if (!boxed || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    invoke_listener(env, state, boxed, nullptr);
    env->DeleteLocalRef(boxed);
    detach_env(ctx->jvm, did_attach);
}

void notify_boolean(WebViewContext *ctx, JavaListenerState &state, bool value) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    bool did_attach = false;
    JNIEnv *env = attach_env(ctx->jvm, &did_attach);
    if (!env) {
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Boolean");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jmethodID value_of = env->GetStaticMethodID(cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    if (!value_of) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, value_of, value ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(cls);
    if (!boxed || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    invoke_listener(env, state, boxed, nullptr);
    env->DeleteLocalRef(boxed);
    detach_env(ctx->jvm, did_attach);
}

void notify_boolean_string(WebViewContext *ctx, JavaListenerState &state, bool value, const wchar_t *message) {
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) return;

    bool did_attach = false;
    JNIEnv *env = attach_env(ctx->jvm, &did_attach);
    if (!env) {
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jclass bool_cls = env->FindClass("java/lang/Boolean");
    if (!bool_cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jmethodID bool_value_of = env->GetStaticMethodID(bool_cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    if (!bool_value_of) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(bool_cls);
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jobject boxed_bool = env->CallStaticObjectMethod(bool_cls, bool_value_of, value ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(bool_cls);
    if (!boxed_bool || env->ExceptionCheck()) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(ctx->jvm, did_attach);
        return;
    }

    jstring boxed_message = nullptr;
    if (message) {
        const jchar *chars = reinterpret_cast<const jchar *>(message);
        const jsize len = static_cast<jsize>(wcslen(message));
        boxed_message = env->NewString(chars, len);
        if (!boxed_message || env->ExceptionCheck()) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(boxed_bool);
            detach_env(ctx->jvm, did_attach);
            return;
        }
    }

    invoke_listener(env, state, boxed_bool, boxed_message);
    env->DeleteLocalRef(boxed_bool);
    if (boxed_message) env->DeleteLocalRef(boxed_message);
    detach_env(ctx->jvm, did_attach);
}

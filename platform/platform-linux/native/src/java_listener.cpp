#include "java_listener.h"

namespace wvbridge {

struct JavaListener {
    JavaVM* jvm = nullptr;
    jobject callback = nullptr;
    jmethodID method = nullptr;
    bool use_accept = true;
};

namespace {

JNIEnv* get_env(JavaVM* jvm, bool* did_attach) {
    if (did_attach) *did_attach = false;
    if (!jvm) return nullptr;

    JNIEnv* env = nullptr;
    const jint result = jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (result == JNI_OK) return env;
    if (result == JNI_EDETACHED) {
        if (jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
            return nullptr;
        }
        if (did_attach) *did_attach = true;
        return env;
    }
    return nullptr;
}

void detach_env(JavaVM* jvm, bool did_attach) {
    if (jvm && did_attach) {
        jvm->DetachCurrentThread();
    }
}

void call_listener(JavaListener* listener, jobject arg) {
    if (!listener || !listener->jvm || !listener->callback || !listener->method || !arg) return;

    bool did_attach = false;
    JNIEnv* env = get_env(listener->jvm, &did_attach);
    if (!env) {
        detach_env(listener->jvm, did_attach);
        return;
    }

    if (listener->use_accept) {
        env->CallVoidMethod(listener->callback, listener->method, arg);
    } else {
        jobject result = env->CallObjectMethod(listener->callback, listener->method, arg);
        if (result) env->DeleteLocalRef(result);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(listener->jvm, did_attach);
}

} // namespace

JavaListener* java_listener_new(JNIEnv* env) {
    if (!env) return nullptr;

    auto* listener = new JavaListener();
    if (env->GetJavaVM(&listener->jvm) != JNI_OK) {
        delete listener;
        return nullptr;
    }
    return listener;
}

void java_listener_destroy(JavaListener* listener) {
    if (!listener) return;

    if (listener->callback && listener->jvm) {
        bool did_attach = false;
        JNIEnv* env = get_env(listener->jvm, &did_attach);
        if (env) {
            env->DeleteGlobalRef(listener->callback);
        }
        detach_env(listener->jvm, did_attach);
    }

    delete listener;
}

void java_listener_set(JNIEnv* env, JavaListener* listener, jobject callback) {
    if (!env || !listener) return;

    if (listener->callback) {
        env->DeleteGlobalRef(listener->callback);
        listener->callback = nullptr;
        listener->method = nullptr;
    }

    if (!callback) return;

    jobject global = env->NewGlobalRef(callback);
    if (!global) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }

    jclass cls = env->GetObjectClass(callback);
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    jmethodID method = env->GetMethodID(cls, "accept", "(Ljava/lang/Object;)V");
    bool use_accept = true;
    if (!method) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        method = env->GetMethodID(cls, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;");
        use_accept = false;
    }
    env->DeleteLocalRef(cls);

    if (!method) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteGlobalRef(global);
        return;
    }

    listener->callback = global;
    listener->method = method;
    listener->use_accept = use_accept;
}

void java_listener_notify_string(JavaListener* listener, const char* value) {
    if (!listener || !listener->jvm || !listener->callback || !listener->method) return;

    bool did_attach = false;
    JNIEnv* env = get_env(listener->jvm, &did_attach);
    if (!env) {
        detach_env(listener->jvm, did_attach);
        return;
    }

    jstring boxed = env->NewStringUTF(value ? value : "");
    if (boxed) {
        call_listener(listener, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(listener->jvm, did_attach);
}

void java_listener_notify_float(JavaListener* listener, float value) {
    if (!listener || !listener->jvm || !listener->callback || !listener->method) return;

    bool did_attach = false;
    JNIEnv* env = get_env(listener->jvm, &did_attach);
    if (!env) {
        detach_env(listener->jvm, did_attach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Float");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(listener->jvm, did_attach);
        return;
    }

    jmethodID value_of = env->GetStaticMethodID(cls, "valueOf", "(F)Ljava/lang/Float;");
    if (!value_of) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(listener->jvm, did_attach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, value_of, value);
    env->DeleteLocalRef(cls);
    if (boxed && !env->ExceptionCheck()) {
        call_listener(listener, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(listener->jvm, did_attach);
}

void java_listener_notify_boolean(JavaListener* listener, bool value) {
    if (!listener || !listener->jvm || !listener->callback || !listener->method) return;

    bool did_attach = false;
    JNIEnv* env = get_env(listener->jvm, &did_attach);
    if (!env) {
        detach_env(listener->jvm, did_attach);
        return;
    }

    jclass cls = env->FindClass("java/lang/Boolean");
    if (!cls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        detach_env(listener->jvm, did_attach);
        return;
    }

    jmethodID value_of = env->GetStaticMethodID(cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    if (!value_of) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(cls);
        detach_env(listener->jvm, did_attach);
        return;
    }

    jobject boxed = env->CallStaticObjectMethod(cls, value_of, value ? JNI_TRUE : JNI_FALSE);
    env->DeleteLocalRef(cls);
    if (boxed && !env->ExceptionCheck()) {
        call_listener(listener, boxed);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    detach_env(listener->jvm, did_attach);
}

} // namespace wvbridge

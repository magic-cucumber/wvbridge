#include "wvbridge/javascript.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/logger.h"

#include <cwchar>
#include <vector>

namespace {

constexpr const char* WEB_MESSAGE_CONSUMER_CLASS_NAME =
    "top/kagg886/wvbridge/bridge/WebMessageConsumer";

std::mutex g_web_message_consumer_class_mutex;
jclass g_web_message_consumer_class = nullptr;
jmethodID g_consume_method = nullptr;

void clear_jni_exception(JNIEnv* env) {
    if (env != nullptr && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

jclass get_web_message_consumer_class(JNIEnv* env) {
    if (env == nullptr) return nullptr;

    std::lock_guard<std::mutex> lock(g_web_message_consumer_class_mutex);
    if (g_web_message_consumer_class != nullptr) return g_web_message_consumer_class;

    jclass local_class = env->FindClass(WEB_MESSAGE_CONSUMER_CLASS_NAME);
    if (local_class == nullptr) {
        clear_jni_exception(env);
        return nullptr;
    }

    g_web_message_consumer_class = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    if (g_web_message_consumer_class == nullptr) {
        clear_jni_exception(env);
    }
    return g_web_message_consumer_class;
}

jmethodID get_consume_method(JNIEnv* env) {
    jclass consumer_class = get_web_message_consumer_class(env);
    if (consumer_class == nullptr) return nullptr;

    std::lock_guard<std::mutex> lock(g_web_message_consumer_class_mutex);
    if (g_consume_method == nullptr) {
        g_consume_method = env->GetMethodID(consumer_class, "consume", "(Ljava/lang/String;)V");
        if (g_consume_method == nullptr) {
            clear_jni_exception(env);
        }
    }
    return g_consume_method;
}

jstring new_web_message_string(JNIEnv* env, const char* message) {
    if (env == nullptr) return nullptr;
    return env->NewStringUTF(message != nullptr ? message : "");
}

#if defined(_WIN32)
jstring new_web_message_string(JNIEnv* env, const wchar_t* message) {
    if (env == nullptr) return nullptr;
    const wchar_t* safe_message = message != nullptr ? message : L"";
    return env->NewString(
        reinterpret_cast<const jchar*>(safe_message),
        static_cast<jsize>(wcslen(safe_message))
    );
}
#endif

template <typename Message>
void dispatch_web_message(
    std::mutex& mutex,
    wvbridge::WebMessageHandlers& handlers,
    Message message
) {
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) return;

    jmethodID consume = get_consume_method(env);
    if (consume == nullptr) {
        java_runtime_detach_env(attached);
        return;
    }

    std::vector<jobject> local_handlers;
    {
        std::lock_guard<std::mutex> lock(mutex);
        local_handlers.reserve(handlers.size());
        for (const auto& entry : handlers) {
            jobject local = env->NewLocalRef(entry.second);
            if (local != nullptr) local_handlers.push_back(local);
        }
    }

    jstring value = new_web_message_string(env, message);
    if (value != nullptr) {
        for (jobject handler : local_handlers) {
            env->CallVoidMethod(handler, consume, value);
            clear_jni_exception(env);
        }
        env->DeleteLocalRef(value);
    }

    for (jobject handler : local_handlers) {
        env->DeleteLocalRef(handler);
    }
    java_runtime_detach_env(attached);
}

} // namespace

extern "C" void javascript_on_load(JNIEnv* env) {
    (void) get_consume_method(env);
}

namespace wvbridge {

jlong register_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong& next_handler_id,
    jobject callback
) {
    if (env == nullptr || callback == nullptr) return 0;

    jobject global = env->NewGlobalRef(callback);
    if (global == nullptr) {
        clear_jni_exception(env);
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex);
    const jlong handler_id = next_handler_id++;
    handlers[handler_id] = global;
    return handler_id;
}

void unregister_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong handler_id
) {
    if (env == nullptr) return;

    jobject global = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = handlers.find(handler_id);
        if (it == handlers.end()) return;
        global = it->second;
        handlers.erase(it);
    }
    if (global != nullptr) env->DeleteGlobalRef(global);
}

void delete_web_message_handler_refs(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers
) {
    if (env == nullptr) return;

    std::lock_guard<std::mutex> lock(mutex);
    for (auto& entry : handlers) {
        if (entry.second != nullptr) env->DeleteGlobalRef(entry.second);
    }
    handlers.clear();
}

void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const char* message
) {
    dispatch_web_message(mutex, handlers, message);
}

#if defined(_WIN32)
void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const wchar_t* message
) {
    dispatch_web_message(mutex, handlers, message);
}
#endif

} // namespace wvbridge

#include "wvbridge/javascript.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/logger.h"

#include <cstring>
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
    LOGGER_V("get_web_message_consumer_class: env=%p", static_cast<void*>(env));
    if (env == nullptr) {
        LOGGER_W("get_web_message_consumer_class: env is null");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_web_message_consumer_class_mutex);
    if (g_web_message_consumer_class != nullptr) {
        LOGGER_V("get_web_message_consumer_class: using cached class=%p", static_cast<void*>(g_web_message_consumer_class));
        return g_web_message_consumer_class;
    }

    LOGGER_V("get_web_message_consumer_class: finding class=%s", WEB_MESSAGE_CONSUMER_CLASS_NAME);
    jclass local_class = env->FindClass(WEB_MESSAGE_CONSUMER_CLASS_NAME);
    if (local_class == nullptr) {
        LOGGER_W("get_web_message_consumer_class: FindClass failed, clearing JNI exception");
        clear_jni_exception(env);
        return nullptr;
    }

    LOGGER_V("get_web_message_consumer_class: creating global ref from local class=%p", static_cast<void*>(local_class));
    g_web_message_consumer_class = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    if (g_web_message_consumer_class == nullptr) {
        LOGGER_W("get_web_message_consumer_class: NewGlobalRef failed, clearing JNI exception");
        clear_jni_exception(env);
    } else {
        LOGGER_V("get_web_message_consumer_class: cached class=%p", static_cast<void*>(g_web_message_consumer_class));
    }
    return g_web_message_consumer_class;
}

jmethodID get_consume_method(JNIEnv* env) {
    LOGGER_V("get_consume_method: env=%p", static_cast<void*>(env));
    jclass consumer_class = get_web_message_consumer_class(env);
    if (consumer_class == nullptr) {
        LOGGER_W("get_consume_method: consumer class unavailable");
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_web_message_consumer_class_mutex);
    if (g_consume_method == nullptr) {
        LOGGER_V("get_consume_method: resolving consume(Ljava/lang/String;)V on class=%p", static_cast<void*>(consumer_class));
        g_consume_method = env->GetMethodID(consumer_class, "consume", "(Ljava/lang/String;)V");
        if (g_consume_method == nullptr) {
            LOGGER_W("get_consume_method: GetMethodID failed, clearing JNI exception");
            clear_jni_exception(env);
        } else {
            LOGGER_V("get_consume_method: cached method=%p", reinterpret_cast<void*>(g_consume_method));
        }
    } else {
        LOGGER_V("get_consume_method: using cached method=%p", reinterpret_cast<void*>(g_consume_method));
    }
    return g_consume_method;
}

jstring new_web_message_string(JNIEnv* env, const char* message) {
    LOGGER_V(
        "new_web_message_string: env=%p utf8_message=%p len=%zu preview=%.100s",
        static_cast<void*>(env),
        static_cast<const void*>(message),
        message != nullptr ? std::strlen(message) : 0,
        message != nullptr ? message : ""
    );
    if (env == nullptr) {
        LOGGER_W("new_web_message_string: env is null");
        return nullptr;
    }
    jstring value = env->NewStringUTF(message != nullptr ? message : "");
    LOGGER_V("new_web_message_string: NewStringUTF returned=%p", static_cast<void*>(value));
    return value;
}

#if defined(_WIN32)
jstring new_web_message_string(JNIEnv* env, const wchar_t* message) {
    LOGGER_V(
        "new_web_message_string: env=%p utf16_message=%p len=%zu preview=%.*ls",
        static_cast<void*>(env),
        static_cast<const void*>(message),
        message != nullptr ? wcslen(message) : 0,
        100,
        message != nullptr ? message : L""
    );
    if (env == nullptr) {
        LOGGER_W("new_web_message_string: env is null");
        return nullptr;
    }
    const wchar_t* safe_message = message != nullptr ? message : L"";
    jstring value = env->NewString(
        reinterpret_cast<const jchar*>(safe_message),
        static_cast<jsize>(wcslen(safe_message))
    );
    LOGGER_V("new_web_message_string: NewString returned=%p", static_cast<void*>(value));
    return value;
}
#endif

template <typename Message>
void dispatch_web_message(
    std::mutex& mutex,
    wvbridge::WebMessageHandlers& handlers,
    Message message
) {
    LOGGER_V("dispatch_web_message: entry handlers=%p", static_cast<void*>(&handlers));
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    LOGGER_V("dispatch_web_message: java_runtime_get_env env=%p attached=%d", static_cast<void*>(env), attached);
    if (env == nullptr) {
        LOGGER_W("dispatch_web_message: env unavailable, dropping message");
        return;
    }

    jmethodID consume = get_consume_method(env);
    if (consume == nullptr) {
        LOGGER_W("dispatch_web_message: consume method unavailable, dropping message");
        java_runtime_detach_env(attached);
        return;
    }

    std::vector<jobject> local_handlers;
    {
        std::lock_guard<std::mutex> lock(mutex);
        LOGGER_V("dispatch_web_message: copying handlers count=%zu", handlers.size());
        local_handlers.reserve(handlers.size());
        for (const auto& entry : handlers) {
            jobject local = env->NewLocalRef(entry.second);
            if (local != nullptr) {
                LOGGER_V(
                    "dispatch_web_message: copied handler id=%lld global=%p local=%p",
                    (long long)entry.first,
                    static_cast<void*>(entry.second),
                    static_cast<void*>(local)
                );
                local_handlers.push_back(local);
            } else {
                LOGGER_W(
                    "dispatch_web_message: NewLocalRef failed for handler id=%lld global=%p",
                    (long long)entry.first,
                    static_cast<void*>(entry.second)
                );
                clear_jni_exception(env);
            }
        }
    }
    LOGGER_V("dispatch_web_message: local handler count=%zu", local_handlers.size());

    jstring value = new_web_message_string(env, message);
    if (value != nullptr) {
        LOGGER_V("dispatch_web_message: invoking handlers with value=%p", static_cast<void*>(value));
        size_t index = 0;
        for (jobject handler : local_handlers) {
            LOGGER_V("dispatch_web_message: calling handler index=%zu local=%p", index, static_cast<void*>(handler));
            env->CallVoidMethod(handler, consume, value);
            if (env->ExceptionCheck()) {
                LOGGER_W("dispatch_web_message: handler index=%zu threw, clearing JNI exception", index);
                clear_jni_exception(env);
            } else {
                LOGGER_V("dispatch_web_message: handler index=%zu completed", index);
            }
            ++index;
        }
        LOGGER_V("dispatch_web_message: deleting message local ref=%p", static_cast<void*>(value));
        env->DeleteLocalRef(value);
    } else {
        LOGGER_W("dispatch_web_message: message jstring creation failed, handlers not invoked");
        clear_jni_exception(env);
    }

    for (jobject handler : local_handlers) {
        LOGGER_V("dispatch_web_message: deleting handler local ref=%p", static_cast<void*>(handler));
        env->DeleteLocalRef(handler);
    }
    LOGGER_V("dispatch_web_message: detaching env attached=%d", attached);
    java_runtime_detach_env(attached);
    LOGGER_V("dispatch_web_message: complete");
}

} // namespace

extern "C" void javascript_on_load(JNIEnv* env) {
    LOGGER_V("javascript_on_load: env=%p", static_cast<void*>(env));
    (void) get_consume_method(env);
    LOGGER_V("javascript_on_load: preload complete");
}

namespace wvbridge {

jlong register_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong& next_handler_id,
    jobject callback
) {
    LOGGER_V(
        "register_web_message_handler: env=%p handlers=%p next_handler_id=%lld callback=%p",
        static_cast<void*>(env),
        static_cast<void*>(&handlers),
        (long long)next_handler_id,
        static_cast<void*>(callback)
    );
    if (env == nullptr || callback == nullptr) {
        LOGGER_W("register_web_message_handler: env or callback is null");
        return 0;
    }

    jobject global = env->NewGlobalRef(callback);
    if (global == nullptr) {
        LOGGER_W("register_web_message_handler: NewGlobalRef failed, clearing JNI exception");
        clear_jni_exception(env);
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex);
    const jlong handler_id = next_handler_id++;
    handlers[handler_id] = global;
    LOGGER_V(
        "register_web_message_handler: registered handler_id=%lld global=%p total=%zu next_handler_id=%lld",
        (long long)handler_id,
        static_cast<void*>(global),
        handlers.size(),
        (long long)next_handler_id
    );
    return handler_id;
}

void unregister_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong handler_id
) {
    LOGGER_V(
        "unregister_web_message_handler: env=%p handlers=%p handler_id=%lld",
        static_cast<void*>(env),
        static_cast<void*>(&handlers),
        (long long)handler_id
    );
    if (env == nullptr) {
        LOGGER_W("unregister_web_message_handler: env is null");
        return;
    }

    jobject global = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = handlers.find(handler_id);
        if (it == handlers.end()) {
            LOGGER_V("unregister_web_message_handler: handler_id=%lld not found", (long long)handler_id);
            return;
        }
        global = it->second;
        handlers.erase(it);
        LOGGER_V(
            "unregister_web_message_handler: removed handler_id=%lld global=%p remaining=%zu",
            (long long)handler_id,
            static_cast<void*>(global),
            handlers.size()
        );
    }
    if (global != nullptr) {
        LOGGER_V("unregister_web_message_handler: deleting global ref=%p", static_cast<void*>(global));
        env->DeleteGlobalRef(global);
    }
}

void delete_web_message_handler_refs(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers
) {
    LOGGER_V("delete_web_message_handler_refs: env=%p handlers=%p", static_cast<void*>(env), static_cast<void*>(&handlers));
    if (env == nullptr) {
        LOGGER_W("delete_web_message_handler_refs: env is null");
        return;
    }

    std::lock_guard<std::mutex> lock(mutex);
    LOGGER_V("delete_web_message_handler_refs: deleting count=%zu", handlers.size());
    for (auto& entry : handlers) {
        if (entry.second != nullptr) {
            LOGGER_V(
                "delete_web_message_handler_refs: deleting handler_id=%lld global=%p",
                (long long)entry.first,
                static_cast<void*>(entry.second)
            );
            env->DeleteGlobalRef(entry.second);
        }
    }
    handlers.clear();
    LOGGER_V("delete_web_message_handler_refs: complete");
}

void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const char* message
) {
    LOGGER_V(
        "dispatch_web_message_to_java: utf8 message=%p len=%zu preview=%.100s",
        static_cast<const void*>(message),
        message != nullptr ? std::strlen(message) : 0,
        message != nullptr ? message : ""
    );
    dispatch_web_message(mutex, handlers, message);
}

#if defined(_WIN32)
void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const wchar_t* message
) {
    LOGGER_V(
        "dispatch_web_message_to_java: utf16 message=%p len=%zu preview=%.*ls",
        static_cast<const void*>(message),
        message != nullptr ? wcslen(message) : 0,
        100,
        message != nullptr ? message : L""
    );
    dispatch_web_message(mutex, handlers, message);
}
#endif

} // namespace wvbridge

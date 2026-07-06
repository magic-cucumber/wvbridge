#include "listener_support.h"

#include "wvbridge/java_runtime.h"
#include "wvbridge/native_bridge.h"
#include <wvbridge/logger.h>

#include <cstdlib>
#include <cstring>

namespace {
JvmStaticCallback g_navigation_interceptor_callback;
}

char* notify_navigation_interceptor_to_jvm(jlong pointer, wvbridge_native_string url) {
    LOGGER_I("notify_navigation_interceptor_to_jvm: pointer=%lld", (long long)pointer);
    int attached = 0;
    JNIEnv* env = java_runtime_get_env(&attached);
    if (env == nullptr) {
        LOGGER_W("notify_navigation_interceptor_to_jvm: failed to get JNIEnv");
        return nullptr;
    }

    char* result = nullptr;
    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_navigation_interceptor_callback,
        "onNavigationInterceptorCallback",
        "(JLjava/lang/String;)Ljava/lang/String;",
        &callback_class
    );
    if (method != nullptr && callback_class != nullptr) {
        LOGGER_V("notify_navigation_interceptor_to_jvm: callback resolved, creating url string");
        jstring value = new_jvm_string(env, url);
        if (value != nullptr) {
            auto response = static_cast<jstring>(
                env->CallStaticObjectMethod(callback_class, method, pointer, value)
            );
            clear_jni_exception(env);
            env->DeleteLocalRef(value);

            if (response != nullptr) {
                const char* chars = env->GetStringUTFChars(response, nullptr);
                if (chars != nullptr) {
                    const size_t size = std::strlen(chars) + 1;
                    result = static_cast<char*>(std::malloc(size));
                    if (result != nullptr) {
                        std::memcpy(result, chars, size);
                        LOGGER_I("notify_navigation_interceptor_to_jvm: result=%s", result);
                    } else {
                        LOGGER_W("notify_navigation_interceptor_to_jvm: malloc failed for result copy");
                    }
                    env->ReleaseStringUTFChars(response, chars);
                }
                env->DeleteLocalRef(response);
            } else {
                LOGGER_W("notify_navigation_interceptor_to_jvm: JVM callback returned null");
            }
        } else {
            LOGGER_W("notify_navigation_interceptor_to_jvm: failed to create JVM url string");
        }
    } else {
        LOGGER_W("notify_navigation_interceptor_to_jvm: callback method/class unavailable");
    }

    java_runtime_detach_env(attached);
    return result;
}

void free_navigation_interceptor_result(char* value) {
    std::free(value);
}

#include "listener_support.h"

#include <cstring>

#if defined(_WIN32)
#include <cwchar>
#else
#include <cstdint>
#include <string>
#endif

#if !defined(_WIN32)
namespace {

std::u16string utf8_to_utf16(const char* value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value);
    std::u16string result;

    while (*bytes != 0) {
        uint32_t code_point = 0;
        size_t length = 0;
        const unsigned char first = bytes[0];

        if (first < 0x80) {
            code_point = first;
            length = 1;
        } else if ((first & 0xE0) == 0xC0) {
            code_point = first & 0x1F;
            length = 2;
        } else if ((first & 0xF0) == 0xE0) {
            code_point = first & 0x0F;
            length = 3;
        } else if ((first & 0xF8) == 0xF0) {
            code_point = first & 0x07;
            length = 4;
        } else {
            result.push_back(u'\uFFFD');
            ++bytes;
            continue;
        }

        bool valid = true;
        for (size_t index = 1; index < length; ++index) {
            const unsigned char next = bytes[index];
            if (next == 0 || (next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            code_point = (code_point << 6) | (next & 0x3F);
        }

        const bool overlong =
            (length == 2 && code_point < 0x80) ||
            (length == 3 && code_point < 0x800) ||
            (length == 4 && code_point < 0x10000);
        if (!valid || overlong || code_point > 0x10FFFF ||
            (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            result.push_back(u'\uFFFD');
            ++bytes;
            continue;
        }

        bytes += length;
        if (code_point <= 0xFFFF) {
            result.push_back(static_cast<char16_t>(code_point));
        } else {
            code_point -= 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (code_point >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (code_point & 0x3FF)));
        }
    }
    return result;
}

} // namespace
#endif

void clear_jni_exception(JNIEnv* env) {
    if (env != nullptr && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

void set_jvm_listener(
    JNIEnv* env,
    JvmListener& listener,
    jobject callback,
    const char* method_name,
    const char* signature
) {
    if (env == nullptr || callback == nullptr || method_name == nullptr || signature == nullptr) {
        return;
    }

    jclass callback_class = env->GetObjectClass(callback);
    if (callback_class == nullptr) {
        clear_jni_exception(env);
        return;
    }

    jmethodID method = env->GetMethodID(callback_class, method_name, signature);
    env->DeleteLocalRef(callback_class);
    if (method == nullptr) {
        clear_jni_exception(env);
        return;
    }

    jobject global_callback = env->NewGlobalRef(callback);
    if (global_callback == nullptr) {
        clear_jni_exception(env);
        return;
    }

    jobject old_callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(listener.mutex);
        old_callback = listener.callback;
        listener.callback = global_callback;
        listener.method = method;
    }
    if (old_callback != nullptr) {
        env->DeleteGlobalRef(old_callback);
    }
}

jobject acquire_jvm_listener(JNIEnv* env, JvmListener& listener, jmethodID* method) {
    if (env == nullptr || method == nullptr) return nullptr;

    std::lock_guard<std::mutex> lock(listener.mutex);
    if (listener.callback == nullptr || listener.method == nullptr) return nullptr;

    jobject callback = env->NewLocalRef(listener.callback);
    if (callback == nullptr) {
        clear_jni_exception(env);
        return nullptr;
    }
    *method = listener.method;
    return callback;
}

#if !defined(_WIN32)
jstring new_jvm_string(JNIEnv* env, const char* value) {
    if (env == nullptr) return nullptr;
    const char* safe_value = value != nullptr ? value : "";
    const std::u16string utf16 = utf8_to_utf16(safe_value);
    jstring result = env->NewString(
        reinterpret_cast<const jchar*>(utf16.data()),
        static_cast<jsize>(utf16.size())
    );
    if (result == nullptr) clear_jni_exception(env);
    return result;
}
#else
jstring new_jvm_string(JNIEnv* env, const wchar_t* value) {
    if (env == nullptr) return nullptr;
    const wchar_t* safe_value = value != nullptr ? value : L"";
    jstring result = env->NewString(
        reinterpret_cast<const jchar*>(safe_value),
        static_cast<jsize>(std::wcslen(safe_value))
    );
    if (result == nullptr) clear_jni_exception(env);
    return result;
}
#endif

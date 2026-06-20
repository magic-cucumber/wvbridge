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

constexpr const char* NATIVE_BRIDGE_CLASS_NAME =
    "top/kagg886/wvbridge/internal/listener/NativeBridge";

std::mutex g_native_bridge_class_mutex;
jclass g_native_bridge_class = nullptr;

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
#else
namespace {

constexpr const char* NATIVE_BRIDGE_CLASS_NAME =
    "top/kagg886/wvbridge/internal/listener/NativeBridge";

std::mutex g_native_bridge_class_mutex;
jclass g_native_bridge_class = nullptr;

} // namespace
#endif

void clear_jni_exception(JNIEnv* env) {
    if (env != nullptr && env->ExceptionCheck()) {
        env->ExceptionClear();
    }
}

namespace {

jclass get_native_bridge_class(JNIEnv* env) {
    if (env == nullptr) return nullptr;

    std::lock_guard<std::mutex> lock(g_native_bridge_class_mutex);
    if (g_native_bridge_class != nullptr) return g_native_bridge_class;

    jclass local_class = env->FindClass(NATIVE_BRIDGE_CLASS_NAME);
    if (local_class == nullptr) {
        clear_jni_exception(env);
        return nullptr;
    }

    g_native_bridge_class = static_cast<jclass>(env->NewGlobalRef(local_class));
    env->DeleteLocalRef(local_class);
    if (g_native_bridge_class == nullptr) {
        clear_jni_exception(env);
    }
    return g_native_bridge_class;
}

} // namespace

extern "C" void listener_support_on_load(JNIEnv* env) {
    (void) get_native_bridge_class(env);
}

jmethodID acquire_native_bridge_callback(
    JNIEnv* env,
    JvmStaticCallback& callback,
    const char* method_name,
    const char* signature,
    jclass* callback_class
) {
    if (callback_class != nullptr) {
        *callback_class = nullptr;
    }
    if (env == nullptr || method_name == nullptr || signature == nullptr || callback_class == nullptr) {
        return nullptr;
    }

    jclass bridge_class = get_native_bridge_class(env);
    if (bridge_class == nullptr) return nullptr;

    std::lock_guard<std::mutex> lock(callback.mutex);
    if (callback.method == nullptr) {
        callback.method = env->GetStaticMethodID(bridge_class, method_name, signature);
        if (callback.method == nullptr) {
            clear_jni_exception(env);
            return nullptr;
        }
    }

    *callback_class = bridge_class;
    return callback.method;
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

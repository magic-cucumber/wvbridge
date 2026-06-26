#include "wvbridge/logger.h"

#include "listener_support.h"
#include "wvbridge/java_runtime.h"

#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct LoggerMessage {
    std::string level;
    std::string tag;
    std::string message;
};

JvmStaticCallback g_logger_callback;
std::thread g_logger_thread;
std::mutex g_logger_thread_mutex;
std::mutex g_logger_queue_mutex;
std::condition_variable g_logger_queue_changed;
std::deque<LoggerMessage> g_logger_queue;
bool g_logger_shutdown = true;
bool g_logger_cleanup_registered = false;

const char* file_name_from_path(const char* file) {
    if (file == nullptr) return "";

    const char* name = file;
    for (const char* cursor = file; *cursor != 0; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            name = cursor + 1;
        }
    }
    return name;
}

std::u16string utf8_to_utf16(const char* value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value != nullptr ? value : "");
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

jstring new_jvm_utf8_string(JNIEnv* env, const std::string& value) {
    if (env == nullptr) return nullptr;
    const std::u16string utf16 = utf8_to_utf16(value.c_str());
    jstring result = env->NewString(
        reinterpret_cast<const jchar*>(utf16.data()),
        static_cast<jsize>(utf16.size())
    );
    if (result == nullptr) clear_jni_exception(env);
    return result;
}

std::string format_logger_message(const char* format, va_list args) {
    if (format == nullptr) return "";

    va_list length_args;
    va_copy(length_args, args);
    const int length = std::vsnprintf(nullptr, 0, format, length_args);
    va_end(length_args);

    if (length < 0) {
        return format;
    }

    std::vector<char> buffer(static_cast<size_t>(length) + 1);
    std::vsnprintf(buffer.data(), buffer.size(), format, args);
    return std::string(buffer.data(), static_cast<size_t>(length));
}

void post_logger_to_jvm(JNIEnv* env, const LoggerMessage& message) {
    jclass callback_class = nullptr;
    jmethodID method = acquire_native_bridge_callback(
        env,
        g_logger_callback,
        "onNativeLoggerPostedCallback",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        &callback_class
    );
    if (method == nullptr || callback_class == nullptr) return;

    jstring level = new_jvm_utf8_string(env, message.level);
    jstring tag = new_jvm_utf8_string(env, message.tag);
    jstring text = new_jvm_utf8_string(env, message.message);
    if (level != nullptr && tag != nullptr && text != nullptr) {
        env->CallStaticVoidMethod(callback_class, method, level, tag, text);
        clear_jni_exception(env);
    }

    if (level != nullptr) env->DeleteLocalRef(level);
    if (tag != nullptr) env->DeleteLocalRef(tag);
    if (text != nullptr) env->DeleteLocalRef(text);
}

JNIEnv* get_logger_env(bool* attached) {
    if (attached != nullptr) {
        *attached = false;
    }

    JavaVM* vm = java_runtime_get_vm();
    if (vm == nullptr) return nullptr;

    JNIEnv* env = nullptr;
    const jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_OK) return env;
    if (status != JNI_EDETACHED) return nullptr;

    if (vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
        return nullptr;
    }
    if (attached != nullptr) {
        *attached = true;
    }
    return env;
}

void logger_loop() {
    JNIEnv* env = nullptr;
    bool attached = false;

    for (;;) {
        LoggerMessage message;
        {
            std::unique_lock<std::mutex> lock(g_logger_queue_mutex);
            g_logger_queue_changed.wait(lock, [] {
                return g_logger_shutdown || !g_logger_queue.empty();
            });
            if (g_logger_shutdown && g_logger_queue.empty()) {
                break;
            }
            message = std::move(g_logger_queue.front());
            g_logger_queue.pop_front();
        }

        if (env == nullptr) {
            env = get_logger_env(&attached);
        }
        if (env == nullptr) continue;

        post_logger_to_jvm(env, message);
    }

    // During VM/library shutdown, DetachCurrentThread can block behind the
    // VM_Exit safepoint while the VM thread is waiting for this logger thread
    // to join. Let the process teardown reclaim the daemon attachment instead.
    bool should_detach = true;
    {
        std::lock_guard<std::mutex> lock(g_logger_queue_mutex);
        should_detach = !g_logger_shutdown;
    }
    if (attached && should_detach) {
        JavaVM* vm = java_runtime_get_vm();
        if (vm != nullptr) {
            vm->DetachCurrentThread();
        }
    }
}

} // namespace

extern "C" void notify_jvm_logger(
    const char* level,
    const char* tag,
    const char* format,
    ...
) {
    va_list args;
    va_start(args, format);
    std::string message = format_logger_message(format, args);
    va_end(args);

    {
        std::lock_guard<std::mutex> lock(g_logger_queue_mutex);
        if (g_logger_shutdown) {
            return;
        }
        g_logger_queue.push_back(LoggerMessage {
            level != nullptr ? level : "",
            tag != nullptr ? tag : "",
            std::move(message)
        });
    }
    g_logger_queue_changed.notify_one();
}

extern "C" const char* logger_location_tag(const char* file, int line) {
    thread_local std::string tag;
    tag = file_name_from_path(file);
    tag += ":";
    tag += std::to_string(line);
    return tag.c_str();
}

extern "C" void logger_on_load() {
    std::lock_guard<std::mutex> thread_lock(g_logger_thread_mutex);

    if (!g_logger_cleanup_registered) {
        std::atexit(logger_on_unload);
        g_logger_cleanup_registered = true;
    }

    {
        std::lock_guard<std::mutex> queue_lock(g_logger_queue_mutex);
        g_logger_shutdown = false;
    }

    if (!g_logger_thread.joinable()) {
        g_logger_thread = std::thread(logger_loop);
    }
}

extern "C" void logger_on_unload() {
    std::lock_guard<std::mutex> thread_lock(g_logger_thread_mutex);

    {
        std::lock_guard<std::mutex> lock(g_logger_queue_mutex);
        g_logger_shutdown = true;
        g_logger_queue.clear();
    }
    g_logger_queue_changed.notify_all();

    if (g_logger_thread.joinable()) {
        g_logger_thread.join();
    }
}

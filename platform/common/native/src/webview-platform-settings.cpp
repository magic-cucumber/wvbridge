#include "wvbridge/webview-platform-settings.h"

#include <cstring>

namespace {

bool check_class(JNIEnv *env, jobject object, const char *class_name) {
    if (!object) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "platform setting is null");
        return false;
    }

    jclass expected = env->FindClass(class_name);
    if (!expected) {
        return false;
    }
    if (!env->IsInstanceOf(object, expected)) {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "platform setting class does not match current platform");
        return false;
    }
    return true;
}

std::string get_nullable_string_field(JNIEnv *env, jobject object, const char *name) {
    jclass cls = env->GetObjectClass(object);
    jfieldID field = env->GetFieldID(cls, name, "Ljava/lang/String;");
    if (!field || env->ExceptionCheck()) {
        return "";
    }

    auto value = static_cast<jstring>(env->GetObjectField(object, field));
    if (!value) {
        return "";
    }

    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

#if defined(__APPLE__)
std::string get_enum_name(JNIEnv *env, jobject enum_value) {
    if (!enum_value) {
        return "";
    }
    jclass enum_class = env->GetObjectClass(enum_value);
    jmethodID name_method = env->GetMethodID(enum_class, "name", "()Ljava/lang/String;");
    if (!name_method || env->ExceptionCheck()) {
        return "";
    }

    auto name = static_cast<jstring>(env->CallObjectMethod(enum_value, name_method));
    if (!name || env->ExceptionCheck()) {
        return "";
    }

    const char *chars = env->GetStringUTFChars(name, nullptr);
    if (!chars) {
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(name, chars);
    return result;
}
#endif

}

#if defined(_WIN32)
bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeWindowsWebViewPlatformSetting *out) {
    if (!out || !check_class(env, setting, "top/kagg886/wvbridge/config/internal/NativeWindowsWebViewPlatformSetting")) {
        return false;
    }
    out->user_agent = get_nullable_string_field(env, setting, "userAgent");
    out->data_dir = get_nullable_string_field(env, setting, "dataDir");
    if (out->data_dir.empty()) {
        env->ThrowNew(env->FindClass("java/lang/IllegalArgumentException"), "Windows WebView2 dataDir must not be empty");
        return false;
    }
    return !env->ExceptionCheck();
}
#elif defined(__APPLE__)
bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeMacOSWebViewPlatformSetting *out) {
    if (!out || !check_class(env, setting, "top/kagg886/wvbridge/config/internal/NativeMacOSWebViewPlatformSetting")) {
        return false;
    }
    out->user_agent = get_nullable_string_field(env, setting, "userAgent");

    jclass cls = env->GetObjectClass(setting);
    jfieldID field = env->GetFieldID(cls, "websiteDataStore", "Ltop/kagg886/wvbridge/config/internal/NativeMacOSWebViewWebsiteDataStore;");
    if (!field || env->ExceptionCheck()) {
        return false;
    }
    jobject enum_value = env->GetObjectField(setting, field);
    const std::string name = get_enum_name(env, enum_value);
    out->website_data_store = name == "NON_PERSISTENT"
        ? WvBridgeWebsiteDataStore::NON_PERSISTENT
        : WvBridgeWebsiteDataStore::DEFAULT;
    return !env->ExceptionCheck();
}
#else
bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeLinuxWebViewPlatformSetting *out) {
    if (!out || !check_class(env, setting, "top/kagg886/wvbridge/config/internal/NativeLinuxWebViewPlatformSetting")) {
        return false;
    }
    out->user_agent = get_nullable_string_field(env, setting, "userAgent");
    out->data_dir = get_nullable_string_field(env, setting, "dataDir");
    out->cache_dir = get_nullable_string_field(env, setting, "cacheDir");
    return !env->ExceptionCheck();
}
#endif

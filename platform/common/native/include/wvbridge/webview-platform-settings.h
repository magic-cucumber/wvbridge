#pragma once

#include <jni.h>
#include <string>

#if defined(_WIN32)
struct WvBridgeWindowsWebViewPlatformSetting {
    std::string user_agent;
    std::string data_dir;
};

bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeWindowsWebViewPlatformSetting *out);
#elif defined(__APPLE__)
enum class WvBridgeWebsiteDataStore {
    DEFAULT,
    NON_PERSISTENT
};

struct WvBridgeMacOSWebViewPlatformSetting {
    std::string user_agent;
    WvBridgeWebsiteDataStore website_data_store = WvBridgeWebsiteDataStore::DEFAULT;
};

bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeMacOSWebViewPlatformSetting *out);
#else
struct WvBridgeLinuxWebViewPlatformSetting {
    std::string user_agent;
    std::string data_dir;
    std::string cache_dir;
};

bool parse_webview_platform_settings(JNIEnv *env, jobject setting, WvBridgeLinuxWebViewPlatformSetting *out);
#endif

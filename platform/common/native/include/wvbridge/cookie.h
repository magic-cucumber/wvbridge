#pragma once

#include <jni.h>

#include <map>
#include <string>
#include <vector>

namespace wvbridge {

using CookieProperties = std::map<std::string, std::string>;
using CookiePropertiesList = std::vector<CookieProperties>;

inline constexpr const char *COOKIE_NAME = "name";
inline constexpr const char *COOKIE_VALUE = "value";
inline constexpr const char *COOKIE_DOMAIN = "domain";
inline constexpr const char *COOKIE_PATH = "path";
inline constexpr const char *COOKIE_EXPIRES = "expires";
inline constexpr const char *COOKIE_HTTP_ONLY = "httpOnly";
inline constexpr const char *COOKIE_SECURE = "secure";
inline constexpr const char *COOKIE_SESSION = "session";
inline constexpr const char *COOKIE_SAME_SITE = "sameSite";

struct CookieUri {
    std::string scheme;
    std::string host;
    std::string path;
};

bool is_structed_cookie(JNIEnv *env, jobject cookie);
bool java_cookie_to_properties(JNIEnv *env, jobject cookie, CookieProperties *out);
jobject new_structed_cookie(JNIEnv *env, const CookieProperties &properties);
jobjectArray new_structed_cookie_array(JNIEnv *env, const CookiePropertiesList &cookies);

std::string cookie_property(
    const CookieProperties &properties,
    const char *name,
    const std::string &fallback = ""
);
bool cookie_property_bool(const CookieProperties &properties, const char *name, bool fallback = false);
bool parse_cookie_uri(const std::string &uri, CookieUri *out, std::string *error = nullptr);
std::string default_cookie_path(const std::string &request_path);
bool cookie_applies_to_uri(const CookieProperties &properties, const CookieUri &uri);

} // namespace wvbridge

// Platform adapters provide the mapper. Native platform cookie handles never cross JNI.
#define WVBRIDGE_PLATFORM_COOKIE_TO_JNI(env, platform_cookie, mapper) \
    ::wvbridge::new_structed_cookie((env), (mapper)(platform_cookie))

#define WVBRIDGE_JNI_COOKIE_TO_PLATFORM(env, java_cookie, mapper) \
    (mapper)((env), (java_cookie))

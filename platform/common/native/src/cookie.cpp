#include "wvbridge/cookie.h"

#include <wvbridge/logger.h>

#include <algorithm>
#include <cctype>

namespace {

constexpr const char *STRUCTED_COOKIE_CLASS =
    "top/kagg886/wvbridge/internal/cookie/StructedCookie";

void throw_illegal_argument(JNIEnv *env, const char *message) {
    LOGGER_E("cookie.throwIllegalArgument: enter env=%p message=%s", env, message ? message : "");
    if (!env) {
        LOGGER_W("cookie.throwIllegalArgument: exit without throw because env is null");
        return;
    }
    if (env->ExceptionCheck()) {
        LOGGER_W("cookie.throwIllegalArgument: exit without throw because exception already pending");
        return;
    }
    jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");
    if (exception_class) {
        env->ThrowNew(exception_class, message);
        LOGGER_E("cookie.throwIllegalArgument: exit thrown message=%s", message ? message : "");
    } else {
        LOGGER_E("cookie.throwIllegalArgument: exit failed to find exception class");
    }
}

std::string jstring_to_string(JNIEnv *env, jstring value) {
    LOGGER_V("cookie.jstringToString: enter env=%p value=%p", env, value);
    if (!value) {
        LOGGER_W("cookie.jstringToString: value is null, returning empty string");
        return "";
    }
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        LOGGER_E("cookie.jstringToString: GetStringUTFChars returned null");
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    LOGGER_D("cookie.jstringToString: converted length=%zu", result.size());
    return result;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool domain_matches(const std::string &host_value, const std::string &domain_value) {
    LOGGER_V("cookie.domainMatches: enter host=%s domain=%s", host_value.c_str(), domain_value.c_str());
    const std::string host = lower_ascii(host_value);
    const std::string domain = lower_ascii(domain_value);
    if (host.empty() || domain.empty()) {
        LOGGER_V("cookie.domainMatches: exit false because host/domain empty");
        return false;
    }
    if (domain.front() != '.') {
        const bool matches = host == domain;
        LOGGER_V("cookie.domainMatches: exact-domain exit matches=%d", matches);
        return matches;
    }

    const std::string suffix = domain.substr(1);
    if (host == suffix) {
        LOGGER_V("cookie.domainMatches: suffix-equal exit true");
        return true;
    }
    const bool matches = host.size() > domain.size() &&
        host.compare(host.size() - domain.size(), domain.size(), domain) == 0;
    LOGGER_V("cookie.domainMatches: dotted-domain exit matches=%d", matches);
    return matches;
}

bool path_matches(const std::string &request_path_value, const std::string &cookie_path_value) {
    LOGGER_V("cookie.pathMatches: enter request_path=%s cookie_path=%s", request_path_value.c_str(), cookie_path_value.c_str());
    const std::string request_path = request_path_value.empty() ? "/" : request_path_value;
    const std::string cookie_path = cookie_path_value.empty() ? "/" : cookie_path_value;
    if (request_path == cookie_path) {
        LOGGER_V("cookie.pathMatches: exact exit true");
        return true;
    }
    if (request_path.size() < cookie_path.size() ||
        request_path.compare(0, cookie_path.size(), cookie_path) != 0) {
        LOGGER_V("cookie.pathMatches: prefix exit false");
        return false;
    }
    const bool matches = cookie_path.back() == '/' || request_path[cookie_path.size()] == '/';
    LOGGER_V("cookie.pathMatches: boundary exit matches=%d", matches);
    return matches;
}

} // namespace

namespace wvbridge {

bool is_structed_cookie(JNIEnv *env, jobject cookie) {
    LOGGER_V("cookie.isStructedCookie: enter env=%p cookie=%p", env, cookie);
    if (!env || !cookie) {
        throw_illegal_argument(env, "cookie must be a StructedCookie");
        LOGGER_E("cookie.isStructedCookie: exit false because env/cookie invalid");
        return false;
    }
    jclass cookie_class = env->FindClass(STRUCTED_COOKIE_CLASS);
    if (!cookie_class) {
        LOGGER_E("cookie.isStructedCookie: exit false because class lookup failed");
        return false;
    }
    const bool matches = env->IsInstanceOf(cookie, cookie_class) == JNI_TRUE;
    env->DeleteLocalRef(cookie_class);
    if (!matches) throw_illegal_argument(env, "cookie must be a StructedCookie");
    LOGGER_D("cookie.isStructedCookie: exit matches=%d", matches);
    return matches;
}

bool java_cookie_to_properties(JNIEnv *env, jobject cookie, CookieProperties *out) {
    LOGGER_I("cookie.javaToProperties: enter env=%p cookie=%p out=%p", env, cookie, out);
    if (!out || !is_structed_cookie(env, cookie)) {
        LOGGER_E("cookie.javaToProperties: exit false because output or cookie validation failed");
        return false;
    }

    jclass cookie_class = env->GetObjectClass(cookie);
    jfieldID dict_field = env->GetFieldID(cookie_class, "dict", "Ljava/util/Map;");
    if (!dict_field || env->ExceptionCheck()) {
        env->DeleteLocalRef(cookie_class);
        LOGGER_E("cookie.javaToProperties: exit false because dict field lookup failed");
        return false;
    }
    jobject dict = env->GetObjectField(cookie, dict_field);
    env->DeleteLocalRef(cookie_class);
    if (!dict) {
        throw_illegal_argument(env, "StructedCookie.dict must not be null");
        LOGGER_E("cookie.javaToProperties: exit false because dict is null");
        return false;
    }

    jclass map_class = env->FindClass("java/util/Map");
    jclass set_class = env->FindClass("java/util/Set");
    jclass iterator_class = env->FindClass("java/util/Iterator");
    jclass entry_class = env->FindClass("java/util/Map$Entry");
    jclass string_class = env->FindClass("java/lang/String");
    if (!map_class || !set_class || !iterator_class || !entry_class || !string_class) {
        env->DeleteLocalRef(dict);
        LOGGER_E("cookie.javaToProperties: exit false because collection class lookup failed");
        return false;
    }

    jmethodID entry_set_method = env->GetMethodID(map_class, "entrySet", "()Ljava/util/Set;");
    jmethodID iterator_method = env->GetMethodID(set_class, "iterator", "()Ljava/util/Iterator;");
    jmethodID has_next_method = env->GetMethodID(iterator_class, "hasNext", "()Z");
    jmethodID next_method = env->GetMethodID(iterator_class, "next", "()Ljava/lang/Object;");
    jmethodID get_key_method = env->GetMethodID(entry_class, "getKey", "()Ljava/lang/Object;");
    jmethodID get_value_method = env->GetMethodID(entry_class, "getValue", "()Ljava/lang/Object;");
    if (env->ExceptionCheck()) {
        env->DeleteLocalRef(dict);
        LOGGER_E("cookie.javaToProperties: exit false because map method lookup raised exception");
        return false;
    }

    jobject entries = env->CallObjectMethod(dict, entry_set_method);
    jobject iterator = entries ? env->CallObjectMethod(entries, iterator_method) : nullptr;
    out->clear();
    while (iterator && env->CallBooleanMethod(iterator, has_next_method) == JNI_TRUE) {
        jobject entry = env->CallObjectMethod(iterator, next_method);
        jobject key = entry ? env->CallObjectMethod(entry, get_key_method) : nullptr;
        jobject value = entry ? env->CallObjectMethod(entry, get_value_method) : nullptr;
        if (!key || !value ||
            env->IsInstanceOf(key, string_class) != JNI_TRUE ||
            env->IsInstanceOf(value, string_class) != JNI_TRUE) {
            if (entry) env->DeleteLocalRef(entry);
            if (key) env->DeleteLocalRef(key);
            if (value) env->DeleteLocalRef(value);
            throw_illegal_argument(env, "StructedCookie.dict must contain only String keys and values");
            LOGGER_E("cookie.javaToProperties: invalid entry encountered, breaking map iteration");
            break;
        }
        (*out)[jstring_to_string(env, static_cast<jstring>(key))] =
            jstring_to_string(env, static_cast<jstring>(value));
        env->DeleteLocalRef(value);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(entry);
        if (env->ExceptionCheck()) {
            LOGGER_E("cookie.javaToProperties: exception during map iteration after size=%zu", out->size());
            break;
        }
    }

    if (iterator) env->DeleteLocalRef(iterator);
    if (entries) env->DeleteLocalRef(entries);
    env->DeleteLocalRef(string_class);
    env->DeleteLocalRef(entry_class);
    env->DeleteLocalRef(iterator_class);
    env->DeleteLocalRef(set_class);
    env->DeleteLocalRef(map_class);
    env->DeleteLocalRef(dict);

    if (env->ExceptionCheck()) {
        LOGGER_E("cookie.javaToProperties: exit false because exception is pending after iteration");
        return false;
    }
    if (cookie_property(*out, COOKIE_NAME).empty()) {
        throw_illegal_argument(env, "StructedCookie.name must not be empty");
        LOGGER_E("cookie.javaToProperties: exit false because name is empty");
        return false;
    }
    if (out->find(COOKIE_VALUE) == out->end()) {
        throw_illegal_argument(env, "StructedCookie.value is missing");
        LOGGER_E("cookie.javaToProperties: exit false because value is missing");
        return false;
    }
    LOGGER_D("cookie.javaToProperties: exit true property_count=%zu", out->size());
    return true;
}

jobject new_structed_cookie(JNIEnv *env, const CookieProperties &properties) {
    LOGGER_I("cookie.newStructedCookie: enter env=%p property_count=%zu", env, properties.size());
    if (!env) {
        LOGGER_E("cookie.newStructedCookie: exit null because env is null");
        return nullptr;
    }
    jclass map_class = env->FindClass("java/util/HashMap");
    if (!map_class) {
        LOGGER_E("cookie.newStructedCookie: exit null because HashMap lookup failed");
        return nullptr;
    }
    jmethodID map_constructor = env->GetMethodID(map_class, "<init>", "()V");
    jmethodID put_method = env->GetMethodID(
        map_class,
        "put",
        "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"
    );
    jobject dict = env->NewObject(map_class, map_constructor);
    for (const auto &[key, value] : properties) {
        jstring java_key = env->NewStringUTF(key.c_str());
        jstring java_value = env->NewStringUTF(value.c_str());
        jobject previous = env->CallObjectMethod(dict, put_method, java_key, java_value);
        if (previous) env->DeleteLocalRef(previous);
        env->DeleteLocalRef(java_value);
        env->DeleteLocalRef(java_key);
        if (env->ExceptionCheck()) {
            LOGGER_E("cookie.newStructedCookie: exception while filling map");
            break;
        }
    }
    env->DeleteLocalRef(map_class);
    if (env->ExceptionCheck()) {
        env->DeleteLocalRef(dict);
        LOGGER_E("cookie.newStructedCookie: exit null because exception pending after map fill");
        return nullptr;
    }

    jclass cookie_class = env->FindClass(STRUCTED_COOKIE_CLASS);
    if (!cookie_class) {
        env->DeleteLocalRef(dict);
        LOGGER_E("cookie.newStructedCookie: exit null because StructedCookie lookup failed");
        return nullptr;
    }
    jmethodID constructor = env->GetMethodID(cookie_class, "<init>", "(Ljava/util/Map;)V");
    jobject cookie = constructor ? env->NewObject(cookie_class, constructor, dict) : nullptr;
    env->DeleteLocalRef(cookie_class);
    env->DeleteLocalRef(dict);
    LOGGER_D("cookie.newStructedCookie: exit cookie=%p", cookie);
    return cookie;
}

jobjectArray new_structed_cookie_array(JNIEnv *env, const CookiePropertiesList &cookies) {
    LOGGER_I("cookie.newStructedCookieArray: enter env=%p count=%zu", env, cookies.size());
    if (!env) {
        LOGGER_E("cookie.newStructedCookieArray: exit null because env is null");
        return nullptr;
    }
    jclass cookie_class = env->FindClass(STRUCTED_COOKIE_CLASS);
    if (!cookie_class) {
        LOGGER_E("cookie.newStructedCookieArray: exit null because class lookup failed");
        return nullptr;
    }
    jobjectArray result = env->NewObjectArray(
        static_cast<jsize>(cookies.size()),
        cookie_class,
        nullptr
    );
    env->DeleteLocalRef(cookie_class);
    if (!result) {
        LOGGER_E("cookie.newStructedCookieArray: exit null because array allocation failed");
        return nullptr;
    }

    for (jsize index = 0; index < static_cast<jsize>(cookies.size()); ++index) {
        jobject cookie = new_structed_cookie(env, cookies[static_cast<size_t>(index)]);
        if (!cookie || env->ExceptionCheck()) {
            LOGGER_E("cookie.newStructedCookieArray: exit null at index=%d after element creation", index);
            return nullptr;
        }
        env->SetObjectArrayElement(result, index, cookie);
        env->DeleteLocalRef(cookie);
        if (env->ExceptionCheck()) {
            LOGGER_E("cookie.newStructedCookieArray: exit null at index=%d after SetObjectArrayElement", index);
            return nullptr;
        }
    }
    LOGGER_D("cookie.newStructedCookieArray: exit array=%p count=%zu", result, cookies.size());
    return result;
}

std::string cookie_property(
    const CookieProperties &properties,
    const char *name,
    const std::string &fallback
) {
    const auto value = properties.find(name);
    return value == properties.end() ? fallback : value->second;
}

bool cookie_property_bool(const CookieProperties &properties, const char *name, bool fallback) {
    const auto value = properties.find(name);
    if (value == properties.end()) return fallback;
    const std::string normalized = lower_ascii(value->second);
    if (normalized == "true" || normalized == "1" || normalized == "yes") return true;
    if (normalized == "false" || normalized == "0" || normalized == "no") return false;
    return fallback;
}

bool parse_cookie_uri(const std::string &uri, CookieUri *out, std::string *error) {
    LOGGER_I("cookie.parseUri: enter uri=%s out=%p", uri.c_str(), out);
    if (!out) {
        LOGGER_E("cookie.parseUri: exit false because out is null");
        return false;
    }
    const size_t scheme_end = uri.find("://");
    if (scheme_end == std::string::npos) {
        if (error) *error = "cookie URI must be an absolute HTTP or HTTPS URI";
        LOGGER_E("cookie.parseUri: exit false because scheme delimiter is missing");
        return false;
    }
    const std::string scheme = lower_ascii(uri.substr(0, scheme_end));
    if (scheme != "http" && scheme != "https") {
        if (error) *error = "cookie URI must use HTTP or HTTPS";
        LOGGER_E("cookie.parseUri: exit false because scheme=%s", scheme.c_str());
        return false;
    }

    const size_t authority_start = scheme_end + 3;
    const size_t authority_end = uri.find_first_of("/?#", authority_start);
    std::string authority = uri.substr(
        authority_start,
        authority_end == std::string::npos ? std::string::npos : authority_end - authority_start
    );
    const size_t user_info = authority.rfind('@');
    if (user_info != std::string::npos) authority.erase(0, user_info + 1);

    std::string host;
    if (!authority.empty() && authority.front() == '[') {
        const size_t bracket = authority.find(']');
        if (bracket != std::string::npos) host = authority.substr(0, bracket + 1);
    } else {
        const size_t colon = authority.rfind(':');
        host = colon == std::string::npos ? authority : authority.substr(0, colon);
    }
    if (host.empty()) {
        if (error) *error = "cookie URI must contain a host";
        LOGGER_E("cookie.parseUri: exit false because host is empty");
        return false;
    }

    std::string path = "/";
    if (authority_end != std::string::npos && uri[authority_end] == '/') {
        const size_t path_end = uri.find_first_of("?#", authority_end);
        path = uri.substr(
            authority_end,
            path_end == std::string::npos ? std::string::npos : path_end - authority_end
        );
        if (path.empty()) path = "/";
    }

    out->scheme = scheme;
    out->host = lower_ascii(host);
    out->path = path;
    LOGGER_D("cookie.parseUri: exit true scheme=%s host=%s path=%s", out->scheme.c_str(), out->host.c_str(), out->path.c_str());
    return true;
}

std::string default_cookie_path(const std::string &request_path) {
    LOGGER_V("cookie.defaultPath: enter request_path=%s", request_path.c_str());
    if (request_path.empty() || request_path.front() != '/') {
        LOGGER_V("cookie.defaultPath: exit root because path empty or relative");
        return "/";
    }
    const size_t last_slash = request_path.rfind('/');
    std::string result = last_slash == 0 || last_slash == std::string::npos
        ? "/"
        : request_path.substr(0, last_slash);
    LOGGER_D("cookie.defaultPath: exit result=%s", result.c_str());
    return result;
}

bool cookie_applies_to_uri(const CookieProperties &properties, const CookieUri &uri) {
    LOGGER_V("cookie.appliesToUri: enter host=%s path=%s scheme=%s", uri.host.c_str(), uri.path.c_str(), uri.scheme.c_str());
    if (!domain_matches(uri.host, cookie_property(properties, COOKIE_DOMAIN))) {
        LOGGER_V("cookie.appliesToUri: exit false because domain mismatch");
        return false;
    }
    if (!path_matches(uri.path, cookie_property(properties, COOKIE_PATH, "/"))) {
        LOGGER_V("cookie.appliesToUri: exit false because path mismatch");
        return false;
    }
    const bool secure_ok = !cookie_property_bool(properties, COOKIE_SECURE) || uri.scheme == "https";
    LOGGER_D("cookie.appliesToUri: exit secure_ok=%d", secure_ok);
    return secure_ok;
}

} // namespace wvbridge

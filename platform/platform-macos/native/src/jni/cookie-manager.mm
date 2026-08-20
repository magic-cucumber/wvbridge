#import "javascript-helpers.h"

#include <wvbridge/cookie.h>
#include <wvbridge/logger.h>

#include <cctype>
#include <dispatch/dispatch.h>

namespace {

NSString *nsstring(const std::string &value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

std::string std_string(NSString *value) {
    const char *chars = [value UTF8String];
    return chars ? chars : "";
}

std::string upper_ascii(std::string value) {
    for (char &ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

void wait_for_cookie_operation(dispatch_semaphore_t semaphore) {
    LOGGER_V("macos.cookie.waitOperation: enter semaphore=%p main_thread=%d", semaphore, [NSThread isMainThread]);
    if (![NSThread isMainThread]) {
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        LOGGER_V("macos.cookie.waitOperation: exit after background wait");
        return;
    }
    while (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) {
        LOGGER_V("macos.cookie.waitOperation: pumping run loop while waiting");
        [[NSRunLoop currentRunLoop]
            runMode:NSDefaultRunLoopMode
            beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
    }
    LOGGER_V("macos.cookie.waitOperation: exit after main-thread wait");
}

NSHTTPCookie *create_platform_cookie(
    const wvbridge::CookieProperties &properties,
    const wvbridge::CookieUri &origin
) {
    LOGGER_I("macos.cookie.createPlatformCookie: enter property_count=%zu origin=%s%s", properties.size(), origin.host.c_str(), origin.path.c_str());
    const std::string expires = wvbridge::cookie_property(properties, wvbridge::COOKIE_EXPIRES);
    const bool session = wvbridge::cookie_property_bool(
        properties,
        wvbridge::COOKIE_SESSION,
        expires.empty()
    );

    NSMutableDictionary<NSHTTPCookiePropertyKey, id> *values = [NSMutableDictionary dictionary];
    values[NSHTTPCookieName] = nsstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_NAME)
    );
    values[NSHTTPCookieValue] = nsstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_VALUE)
    );
    values[NSHTTPCookieDomain] = nsstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_DOMAIN, origin.host)
    );
    values[NSHTTPCookiePath] = nsstring(
        wvbridge::cookie_property(
            properties,
            wvbridge::COOKIE_PATH,
            wvbridge::default_cookie_path(origin.path)
        )
    );
    values[NSHTTPCookieVersion] = @0;
    if (wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_HTTP_ONLY)) {
        // Foundation exposes isHTTPOnly but no public NSHTTPCookiePropertyKey constant.
        values[(NSHTTPCookiePropertyKey) @"HttpOnly"] = @"TRUE";
    }
    if (wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_SECURE)) {
        values[NSHTTPCookieSecure] = @"TRUE";
    }
    if (session) {
        values[NSHTTPCookieDiscard] = @"TRUE";
    } else if (!expires.empty()) {
        try {
            values[NSHTTPCookieExpires] = [NSDate dateWithTimeIntervalSince1970:std::stoll(expires) / 1000.0];
        } catch (...) {
            LOGGER_E("macos.cookie.createPlatformCookie: exit nil because expires parse failed value=%s", expires.c_str());
            return nil;
        }
    }

    const std::string same_site = upper_ascii(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_SAME_SITE)
    );
    if (same_site == "NONE") values[NSHTTPCookieSameSitePolicy] = @"None";
    else if (same_site == "STRICT") values[NSHTTPCookieSameSitePolicy] = @"Strict";
    else if (same_site == "LAX") values[NSHTTPCookieSameSitePolicy] = @"Lax";

    NSHTTPCookie *cookie = [[NSHTTPCookie alloc] initWithProperties:values];
    LOGGER_D("macos.cookie.createPlatformCookie: exit cookie=%p session=%d same_site=%s", cookie, session, same_site.c_str());
    return cookie;
}

wvbridge::CookieProperties platform_cookie_to_properties(NSHTTPCookie *cookie) {
    LOGGER_V("macos.cookie.platformToProperties: enter cookie=%p", cookie);
    wvbridge::CookieProperties properties;
    properties[wvbridge::COOKIE_NAME] = std_string(cookie.name);
    properties[wvbridge::COOKIE_VALUE] = std_string(cookie.value);
    properties[wvbridge::COOKIE_DOMAIN] = std_string(cookie.domain);
    properties[wvbridge::COOKIE_PATH] = std_string(cookie.path);
    properties[wvbridge::COOKIE_HTTP_ONLY] = cookie.isHTTPOnly ? "true" : "false";
    properties[wvbridge::COOKIE_SECURE] = cookie.isSecure ? "true" : "false";
    properties[wvbridge::COOKIE_SESSION] = cookie.isSessionOnly ? "true" : "false";
    if (!cookie.isSessionOnly && cookie.expiresDate) {
        properties[wvbridge::COOKIE_EXPIRES] = std::to_string(
            static_cast<long long>(cookie.expiresDate.timeIntervalSince1970 * 1000.0)
        );
    }
    NSString *same_site = cookie.sameSitePolicy;
    if (same_site) properties[wvbridge::COOKIE_SAME_SITE] = upper_ascii(std_string(same_site));
    LOGGER_D("macos.cookie.platformToProperties: exit name=%s domain=%s session=%s", properties[wvbridge::COOKIE_NAME].c_str(), properties[wvbridge::COOKIE_DOMAIN].c_str(), properties[wvbridge::COOKIE_SESSION].c_str());
    return properties;
}

#define PLATFORM_COOKIE_TO_PROPERTIES(cookie) platform_cookie_to_properties(cookie)
#define JNI_TO_PLATFORM_COOKIE(properties, origin) create_platform_cookie((properties), (origin))

bool read_uri(JNIEnv *env, jstring uri, std::string *native_uri, wvbridge::CookieUri *origin) {
    LOGGER_I("macos.cookie.readUri: enter env=%p uri=%p", env, uri);
    if (!uri) {
        LOGGER_E("macos.cookie.readUri: uri is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "uri is null");
        return false;
    }
    NSString *value = jstring_to_nsstring(env, uri);
    if (env->ExceptionCheck()) { LOGGER_E("macos.cookie.readUri: string conversion raised exception"); return false; }
    *native_uri = std_string(value);
    std::string error;
    if (!wvbridge::parse_cookie_uri(*native_uri, origin, &error)) {
        LOGGER_E("macos.cookie.readUri: invalid uri=%s error=%s", native_uri->c_str(), error.c_str());
        throw_jni_exception(env, "java/lang/IllegalArgumentException", error.c_str());
        return false;
    }
    LOGGER_D("macos.cookie.readUri: exit true host=%s path=%s", origin->host.c_str(), origin->path.c_str());
    return true;
}

} // namespace

API_EXPORT(void, putCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("macos.cookie.putCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    @autoreleasepool {
        wvbridge::CookieProperties properties;
        if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) { LOGGER_E("macos.cookie.putCookie: Java cookie conversion failed"); return; }
        std::string native_uri;
        wvbridge::CookieUri origin;
        if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("macos.cookie.putCookie: URI validation failed"); return; }
        auto *ctx = require_context(env, handle, "putCookie");
        if (!ctx) { LOGGER_E("macos.cookie.putCookie: context unavailable"); return; }

        NSHTTPCookie *native_cookie = JNI_TO_PLATFORM_COOKIE(properties, origin);
        if (!native_cookie) {
            LOGGER_E("macos.cookie.putCookie: native cookie creation failed");
            throw_jni_exception(env, "java/lang/IllegalArgumentException", "unable to create HTTPCookie");
            return;
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block bool webview_available = true;
        runOnMainAsync(^{
            LOGGER_V("macos.cookie.putCookie.main: enter ctx=%p webView=%p", ctx, ctx->webView);
            if (!ctx->webView) {
                webview_available = false;
                LOGGER_E("macos.cookie.putCookie.main: webView unavailable");
                dispatch_semaphore_signal(semaphore);
                return;
            }
            WKHTTPCookieStore *store = ctx->webView.configuration.websiteDataStore.httpCookieStore;
            [store setCookie:native_cookie completionHandler:^{
                LOGGER_D("macos.cookie.putCookie.main: setCookie completed cookie=%p", native_cookie);
                dispatch_semaphore_signal(semaphore);
            }];
        });
        wait_for_cookie_operation(semaphore);
        dispatch_release(semaphore);
        [native_cookie release];
        if (!webview_available) {
            LOGGER_E("macos.cookie.putCookie: exit failure webview unavailable");
            throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
            return;
        }
        LOGGER_I("macos.cookie.putCookie: exit success uri=%s", native_uri.c_str());
    }
}

API_EXPORT(void, removeCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("macos.cookie.removeCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    @autoreleasepool {
        wvbridge::CookieProperties properties;
        if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) { LOGGER_E("macos.cookie.removeCookie: Java cookie conversion failed"); return; }
        std::string native_uri;
        wvbridge::CookieUri origin;
        if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("macos.cookie.removeCookie: URI validation failed"); return; }
        auto *ctx = require_context(env, handle, "removeCookie");
        if (!ctx) { LOGGER_E("macos.cookie.removeCookie: context unavailable"); return; }

        NSHTTPCookie *native_cookie = JNI_TO_PLATFORM_COOKIE(properties, origin);
        if (!native_cookie) {
            LOGGER_E("macos.cookie.removeCookie: native cookie creation failed");
            throw_jni_exception(env, "java/lang/IllegalArgumentException", "unable to create HTTPCookie");
            return;
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block bool webview_available = true;
        runOnMainAsync(^{
            LOGGER_V("macos.cookie.removeCookie.main: enter ctx=%p webView=%p", ctx, ctx->webView);
            if (!ctx->webView) {
                webview_available = false;
                LOGGER_E("macos.cookie.removeCookie.main: webView unavailable");
                dispatch_semaphore_signal(semaphore);
                return;
            }
            WKHTTPCookieStore *store = ctx->webView.configuration.websiteDataStore.httpCookieStore;
            [store deleteCookie:native_cookie completionHandler:^{
                LOGGER_D("macos.cookie.removeCookie.main: deleteCookie completed cookie=%p", native_cookie);
                dispatch_semaphore_signal(semaphore);
            }];
        });
        wait_for_cookie_operation(semaphore);
        dispatch_release(semaphore);
        [native_cookie release];
        if (!webview_available) {
            LOGGER_E("macos.cookie.removeCookie: exit failure webview unavailable");
            throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
            return;
        }
        LOGGER_I("macos.cookie.removeCookie: exit success uri=%s", native_uri.c_str());
    }
}

API_EXPORT(void, clearAllCookies, jlong handle) {
    LOGGER_I("macos.cookie.clearAllCookies: enter handle=%lld", (long long) handle);
    @autoreleasepool {
        auto *ctx = require_context(env, handle, "clearAllCookies");
        if (!ctx) { LOGGER_E("macos.cookie.clearAllCookies: context unavailable"); return; }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block bool webview_available = true;
        runOnMainAsync(^{
            LOGGER_V("macos.cookie.clearAllCookies.main: enter ctx=%p webView=%p", ctx, ctx->webView);
            if (!ctx->webView) {
                webview_available = false;
                LOGGER_E("macos.cookie.clearAllCookies.main: webView unavailable");
                dispatch_semaphore_signal(semaphore);
                return;
            }
            WKHTTPCookieStore *store = ctx->webView.configuration.websiteDataStore.httpCookieStore;
            [store getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
                if (cookies.count == 0) {
                    LOGGER_D("macos.cookie.clearAllCookies.main: no cookies to delete");
                    dispatch_semaphore_signal(semaphore);
                    return;
                }
                __block NSUInteger remaining = cookies.count;
                LOGGER_D("macos.cookie.clearAllCookies.main: deleting count=%lu", (unsigned long) remaining);
                for (NSHTTPCookie *cookie in cookies) {
                    [store deleteCookie:cookie completionHandler:^{
                        if (--remaining == 0) { LOGGER_D("macos.cookie.clearAllCookies.main: delete completed for all cookies"); dispatch_semaphore_signal(semaphore); }
                    }];
                }
            }];
        });
        wait_for_cookie_operation(semaphore);
        dispatch_release(semaphore);
        if (!webview_available) {
            LOGGER_E("macos.cookie.clearAllCookies: exit failure webview unavailable");
            throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
            return;
        }
        LOGGER_I("macos.cookie.clearAllCookies: exit success");
    }
}

API_EXPORT(jobjectArray, allCookies, jlong handle, jstring uri) {
    LOGGER_I("macos.cookie.allCookies: enter handle=%lld uri=%p", (long long) handle, uri);
    @autoreleasepool {
        std::string native_uri;
        wvbridge::CookieUri origin;
        if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("macos.cookie.allCookies: URI validation failed"); return nullptr; }
        auto *ctx = require_context(env, handle, "allCookies");
        if (!ctx) { LOGGER_E("macos.cookie.allCookies: context unavailable"); return nullptr; }

        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        auto *result = new wvbridge::CookiePropertiesList();
        __block bool webview_available = true;
        runOnMainAsync(^{
            LOGGER_V("macos.cookie.allCookies.main: enter ctx=%p webView=%p", ctx, ctx->webView);
            if (!ctx->webView) {
                webview_available = false;
                LOGGER_E("macos.cookie.allCookies.main: webView unavailable");
                dispatch_semaphore_signal(semaphore);
                return;
            }
            WKHTTPCookieStore *store = ctx->webView.configuration.websiteDataStore.httpCookieStore;
            [store getAllCookies:^(NSArray<NSHTTPCookie *> *cookies) {
                LOGGER_D("macos.cookie.allCookies.main: fetched count=%lu", (unsigned long) cookies.count);
                for (NSHTTPCookie *cookie in cookies) {
                    wvbridge::CookieProperties properties = PLATFORM_COOKIE_TO_PROPERTIES(cookie);
                    if (wvbridge::cookie_applies_to_uri(properties, origin)) {
                        LOGGER_V("macos.cookie.allCookies.main: cookie matched uri name=%s", properties[wvbridge::COOKIE_NAME].c_str());
                        result->push_back(std::move(properties));
                    } else {
                        LOGGER_V("macos.cookie.allCookies.main: cookie skipped name=%s", properties[wvbridge::COOKIE_NAME].c_str());
                    }
                }
                dispatch_semaphore_signal(semaphore);
            }];
        });
        wait_for_cookie_operation(semaphore);
        dispatch_release(semaphore);
        if (!webview_available) {
            delete result;
            LOGGER_E("macos.cookie.allCookies: exit null webview unavailable");
            throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
            return nullptr;
        }
        jobjectArray output = wvbridge::new_structed_cookie_array(env, *result);
        LOGGER_I("macos.cookie.allCookies: exit output=%p count=%zu", output, result->size());
        delete result;
        return output;
    }
}

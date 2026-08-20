#include "javascript-helpers.h"

#include "gtk.h"

#include <wvbridge/cookie.h>
#include <wvbridge/logger.h>

#include <cctype>
#include <future>

namespace {

struct OperationResult {
    bool ok = false;
    std::string error;
};

struct CookieOperationState {
    std::shared_ptr<std::promise<OperationResult>> completion;
    SoupCookie *cookie = nullptr;
};

struct CookieListResult {
    bool ok = false;
    wvbridge::CookiePropertiesList cookies;
    std::string error;
};

std::string upper_ascii(std::string value) {
    for (char &ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

WebKitWebsiteDataManager *get_data_manager(WebViewContext *ctx) {
    LOGGER_V("linux.cookie.getDataManager: enter ctx=%p webview=%p", ctx, ctx ? ctx->webview : nullptr);
    if (!ctx || !ctx->webview) {
        LOGGER_W("linux.cookie.getDataManager: exit null because context/webview unavailable");
        return nullptr;
    }
    WebKitWebContext *context = webkit_web_view_get_context(ctx->webview);
    WebKitWebsiteDataManager *manager = context ? webkit_web_context_get_website_data_manager(context) : nullptr;
    LOGGER_D("linux.cookie.getDataManager: exit context=%p manager=%p", context, manager);
    return manager;
}

WebKitCookieManager *get_cookie_manager(WebViewContext *ctx) {
    LOGGER_V("linux.cookie.getCookieManager: enter ctx=%p", ctx);
    WebKitWebsiteDataManager *manager = get_data_manager(ctx);
    WebKitCookieManager *cookie_manager = manager ? webkit_website_data_manager_get_cookie_manager(manager) : nullptr;
    LOGGER_D("linux.cookie.getCookieManager: exit manager=%p cookie_manager=%p", manager, cookie_manager);
    return cookie_manager;
}

SoupCookie *create_platform_cookie(
    const wvbridge::CookieProperties &properties,
    const wvbridge::CookieUri &origin
) {
    LOGGER_I("linux.cookie.createPlatformCookie: enter property_count=%zu origin=%s%s", properties.size(), origin.host.c_str(), origin.path.c_str());
    const std::string name = wvbridge::cookie_property(properties, wvbridge::COOKIE_NAME);
    const std::string value = wvbridge::cookie_property(properties, wvbridge::COOKIE_VALUE);
    const std::string domain = wvbridge::cookie_property(
        properties,
        wvbridge::COOKIE_DOMAIN,
        origin.host
    );
    const std::string path = wvbridge::cookie_property(
        properties,
        wvbridge::COOKIE_PATH,
        wvbridge::default_cookie_path(origin.path)
    );
    SoupCookie *cookie = soup_cookie_new(
        name.c_str(),
        value.c_str(),
        domain.c_str(),
        path.c_str(),
        -1
    );
    if (!cookie) {
        LOGGER_E("linux.cookie.createPlatformCookie: exit null because soup_cookie_new failed");
        return nullptr;
    }

    soup_cookie_set_http_only(
        cookie,
        wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_HTTP_ONLY)
    );
    soup_cookie_set_secure(
        cookie,
        wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_SECURE)
    );
    const std::string same_site = upper_ascii(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_SAME_SITE)
    );
    if (same_site == "NONE") {
        soup_cookie_set_same_site_policy(cookie, SOUP_SAME_SITE_POLICY_NONE);
    } else if (same_site == "STRICT") {
        soup_cookie_set_same_site_policy(cookie, SOUP_SAME_SITE_POLICY_STRICT);
    } else if (same_site == "LAX") {
        soup_cookie_set_same_site_policy(cookie, SOUP_SAME_SITE_POLICY_LAX);
    }

    const std::string expires = wvbridge::cookie_property(properties, wvbridge::COOKIE_EXPIRES);
    const bool session = wvbridge::cookie_property_bool(
        properties,
        wvbridge::COOKIE_SESSION,
        expires.empty()
    );
    if (!session && !expires.empty()) {
        try {
            GDateTime *date = g_date_time_new_from_unix_utc(static_cast<gint64>(std::stoll(expires) / 1000));
            if (date) {
                soup_cookie_set_expires(cookie, date);
                g_date_time_unref(date);
                LOGGER_D("linux.cookie.createPlatformCookie: expires applied value=%s", expires.c_str());
            } else {
                LOGGER_W("linux.cookie.createPlatformCookie: expires parsed but GDateTime allocation returned null");
            }
        } catch (...) {
            soup_cookie_free(cookie);
            LOGGER_E("linux.cookie.createPlatformCookie: exit null because expires parse failed value=%s", expires.c_str());
            return nullptr;
        }
    }
    LOGGER_D("linux.cookie.createPlatformCookie: exit cookie=%p session=%d same_site=%s", cookie, session, same_site.c_str());
    return cookie;
}

wvbridge::CookieProperties platform_cookie_to_properties(SoupCookie *cookie) {
    LOGGER_V("linux.cookie.platformToProperties: enter cookie=%p", cookie);
    wvbridge::CookieProperties properties;
    const char *name = soup_cookie_get_name(cookie);
    const char *value = soup_cookie_get_value(cookie);
    const char *domain = soup_cookie_get_domain(cookie);
    const char *path = soup_cookie_get_path(cookie);
    properties[wvbridge::COOKIE_NAME] = name ? name : "";
    properties[wvbridge::COOKIE_VALUE] = value ? value : "";
    properties[wvbridge::COOKIE_DOMAIN] = domain ? domain : "";
    properties[wvbridge::COOKIE_PATH] = path ? path : "/";
    properties[wvbridge::COOKIE_HTTP_ONLY] = soup_cookie_get_http_only(cookie) ? "true" : "false";
    properties[wvbridge::COOKIE_SECURE] = soup_cookie_get_secure(cookie) ? "true" : "false";

    GDateTime *expires = soup_cookie_get_expires(cookie);
    properties[wvbridge::COOKIE_SESSION] = expires ? "false" : "true";
    if (expires) {
        properties[wvbridge::COOKIE_EXPIRES] = std::to_string(g_date_time_to_unix(expires) * 1000);
    }
    switch (soup_cookie_get_same_site_policy(cookie)) {
        case SOUP_SAME_SITE_POLICY_NONE:
            properties[wvbridge::COOKIE_SAME_SITE] = "NONE";
            break;
        case SOUP_SAME_SITE_POLICY_STRICT:
            properties[wvbridge::COOKIE_SAME_SITE] = "STRICT";
            break;
        case SOUP_SAME_SITE_POLICY_LAX:
            properties[wvbridge::COOKIE_SAME_SITE] = "LAX";
            break;
    }
    LOGGER_D("linux.cookie.platformToProperties: exit name=%s domain=%s session=%s", properties[wvbridge::COOKIE_NAME].c_str(), properties[wvbridge::COOKIE_DOMAIN].c_str(), properties[wvbridge::COOKIE_SESSION].c_str());
    return properties;
}

#define PLATFORM_COOKIE_TO_PROPERTIES(cookie) platform_cookie_to_properties(cookie)
#define JNI_TO_PLATFORM_COOKIE(properties, origin) create_platform_cookie((properties), (origin))

bool read_uri(JNIEnv *env, jstring uri, std::string *native_uri, wvbridge::CookieUri *origin) {
    LOGGER_I("linux.cookie.readUri: enter env=%p uri=%p", env, uri);
    if (!uri) {
        LOGGER_E("linux.cookie.readUri: uri is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "uri is null");
        return false;
    }
    *native_uri = jstring_to_string(env, uri);
    if (env->ExceptionCheck()) {
        LOGGER_E("linux.cookie.readUri: exit false because string conversion raised exception");
        return false;
    }
    std::string error;
    if (!wvbridge::parse_cookie_uri(*native_uri, origin, &error)) {
        LOGGER_E("linux.cookie.readUri: invalid uri=%s error=%s", native_uri->c_str(), error.c_str());
        throw_jni_exception(env, "java/lang/IllegalArgumentException", error.c_str());
        return false;
    }
    LOGGER_D("linux.cookie.readUri: exit true uri=%s host=%s", native_uri->c_str(), origin->host.c_str());
    return true;
}

void finish_cookie_operation(
    GObject *object,
    GAsyncResult *result,
    gpointer user_data,
    bool adding
) {
    LOGGER_V("linux.cookie.finishOperation: enter object=%p result=%p state=%p adding=%d", object, result, user_data, adding);
    auto *state = static_cast<CookieOperationState *>(user_data);
    GError *error = nullptr;
    gboolean ok = adding
        ? webkit_cookie_manager_add_cookie_finish(WEBKIT_COOKIE_MANAGER(object), result, &error)
        : webkit_cookie_manager_delete_cookie_finish(WEBKIT_COOKIE_MANAGER(object), result, &error);
    std::string message = error && error->message ? error->message : "cookie operation failed";
    if (error) g_error_free(error);
    if (state->cookie) soup_cookie_free(state->cookie);
    state->completion->set_value(OperationResult{ok == TRUE, ok == TRUE ? "" : message});
    LOGGER_D("linux.cookie.finishOperation: exit ok=%d message=%s", ok == TRUE, message.c_str());
    delete state;
}

} // namespace

API_EXPORT(void, putCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("linux.cookie.putCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    wvbridge::CookieProperties properties;
    if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) {
        LOGGER_E("linux.cookie.putCookie: exit because Java cookie conversion failed");
        return;
    }
    std::string native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) {
        LOGGER_E("linux.cookie.putCookie: exit because URI validation failed");
        return;
    }
    auto *ctx = require_context(env, handle);
    if (!ctx) {
        LOGGER_E("linux.cookie.putCookie: exit because context is unavailable");
        return;
    }

    auto completion = std::make_shared<std::promise<OperationResult>>();
    auto future = completion->get_future();
    const bool dispatched = wvbridge::gtk_run_on_thread_sync([ctx, properties, origin, completion] {
        LOGGER_V("linux.cookie.putCookie.gtk: enter ctx=%p", ctx);
        WebKitCookieManager *manager = get_cookie_manager(ctx);
        SoupCookie *native_cookie = JNI_TO_PLATFORM_COOKIE(properties, origin);
        if (!manager || !native_cookie) {
            if (native_cookie) soup_cookie_free(native_cookie);
            LOGGER_E("linux.cookie.putCookie.gtk: unable to create cookie manager=%p cookie=%p", manager, native_cookie);
            completion->set_value(OperationResult{false, "unable to create WebKitGTK cookie"});
            return;
        }
        auto *state = new CookieOperationState{completion, native_cookie};
        webkit_cookie_manager_add_cookie(
            manager,
            native_cookie,
            nullptr,
            [](GObject *object, GAsyncResult *result, gpointer user_data) {
                finish_cookie_operation(object, result, user_data, true);
            },
            state
        );
    });
    if (!dispatched) {
        LOGGER_E("linux.cookie.putCookie: GTK dispatch failed");
        throw_jni_exception(env, "java/lang/IllegalStateException", "GTK runtime is not available");
        return;
    }
    const OperationResult result = future.get();
    if (!result.ok) {
        LOGGER_E("linux.cookie.putCookie: platform operation failed error=%s", result.error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return;
    }
    LOGGER_I("linux.cookie.putCookie: exit success uri=%s", native_uri.c_str());
}

API_EXPORT(void, removeCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("linux.cookie.removeCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    wvbridge::CookieProperties properties;
    if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) {
        LOGGER_E("linux.cookie.removeCookie: exit because Java cookie conversion failed");
        return;
    }
    std::string native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) {
        LOGGER_E("linux.cookie.removeCookie: exit because URI validation failed");
        return;
    }
    auto *ctx = require_context(env, handle);
    if (!ctx) {
        LOGGER_E("linux.cookie.removeCookie: exit because context is unavailable");
        return;
    }

    auto completion = std::make_shared<std::promise<OperationResult>>();
    auto future = completion->get_future();
    const bool dispatched = wvbridge::gtk_run_on_thread_sync([ctx, properties, origin, completion] {
        LOGGER_V("linux.cookie.removeCookie.gtk: enter ctx=%p", ctx);
        WebKitCookieManager *manager = get_cookie_manager(ctx);
        SoupCookie *native_cookie = JNI_TO_PLATFORM_COOKIE(properties, origin);
        if (!manager || !native_cookie) {
            if (native_cookie) soup_cookie_free(native_cookie);
            LOGGER_E("linux.cookie.removeCookie.gtk: unable to create cookie manager=%p cookie=%p", manager, native_cookie);
            completion->set_value(OperationResult{false, "unable to create WebKitGTK cookie"});
            return;
        }
        auto *state = new CookieOperationState{completion, native_cookie};
        webkit_cookie_manager_delete_cookie(
            manager,
            native_cookie,
            nullptr,
            [](GObject *object, GAsyncResult *result, gpointer user_data) {
                finish_cookie_operation(object, result, user_data, false);
            },
            state
        );
    });
    if (!dispatched) {
        LOGGER_E("linux.cookie.removeCookie: GTK dispatch failed");
        throw_jni_exception(env, "java/lang/IllegalStateException", "GTK runtime is not available");
        return;
    }
    const OperationResult result = future.get();
    if (!result.ok) {
        LOGGER_E("linux.cookie.removeCookie: platform operation failed error=%s", result.error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return;
    }
    LOGGER_I("linux.cookie.removeCookie: exit success uri=%s", native_uri.c_str());
}

API_EXPORT(void, clearAllCookies, jlong handle) {
    LOGGER_I("linux.cookie.clearAllCookies: enter handle=%lld", (long long) handle);
    auto *ctx = require_context(env, handle);
    if (!ctx) {
        LOGGER_E("linux.cookie.clearAllCookies: exit because context is unavailable");
        return;
    }
    auto completion = std::make_shared<std::promise<OperationResult>>();
    auto future = completion->get_future();
    const bool dispatched = wvbridge::gtk_run_on_thread_sync([ctx, completion] {
        LOGGER_V("linux.cookie.clearAllCookies.gtk: enter ctx=%p", ctx);
        WebKitWebsiteDataManager *manager = get_data_manager(ctx);
        if (!manager) {
            LOGGER_E("linux.cookie.clearAllCookies.gtk: website data manager unavailable");
            completion->set_value(OperationResult{false, "website data manager is not available"});
            return;
        }
        auto *holder = new std::shared_ptr<std::promise<OperationResult>>(completion);
        webkit_website_data_manager_clear(
            manager,
            WEBKIT_WEBSITE_DATA_COOKIES,
            0,
            nullptr,
            [](GObject *object, GAsyncResult *result, gpointer user_data) {
                auto *holder = static_cast<std::shared_ptr<std::promise<OperationResult>> *>(user_data);
                GError *error = nullptr;
                const gboolean ok = webkit_website_data_manager_clear_finish(
                    WEBKIT_WEBSITE_DATA_MANAGER(object),
                    result,
                    &error
                );
                std::string message = error && error->message ? error->message : "clear cookies failed";
                if (error) g_error_free(error);
                (*holder)->set_value(OperationResult{ok == TRUE, ok == TRUE ? "" : message});
                LOGGER_D("linux.cookie.clearAllCookies.callback: exit ok=%d message=%s", ok == TRUE, message.c_str());
                delete holder;
            },
            holder
        );
    });
    if (!dispatched) {
        LOGGER_E("linux.cookie.clearAllCookies: GTK dispatch failed");
        throw_jni_exception(env, "java/lang/IllegalStateException", "GTK runtime is not available");
        return;
    }
    const OperationResult result = future.get();
    if (!result.ok) {
        LOGGER_E("linux.cookie.clearAllCookies: platform operation failed error=%s", result.error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return;
    }
    LOGGER_I("linux.cookie.clearAllCookies: exit success");
}

API_EXPORT(jobjectArray, allCookies, jlong handle, jstring uri) {
    LOGGER_I("linux.cookie.allCookies: enter handle=%lld uri=%p", (long long) handle, uri);
    std::string native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) {
        LOGGER_E("linux.cookie.allCookies: exit null because URI validation failed");
        return nullptr;
    }
    auto *ctx = require_context(env, handle);
    if (!ctx) {
        LOGGER_E("linux.cookie.allCookies: exit null because context is unavailable");
        return nullptr;
    }

    auto completion = std::make_shared<std::promise<CookieListResult>>();
    auto future = completion->get_future();
    const bool dispatched = wvbridge::gtk_run_on_thread_sync([ctx, native_uri, completion] {
        LOGGER_V("linux.cookie.allCookies.gtk: enter ctx=%p uri=%s", ctx, native_uri.c_str());
        WebKitCookieManager *manager = get_cookie_manager(ctx);
        if (!manager) {
            LOGGER_E("linux.cookie.allCookies.gtk: cookie manager unavailable");
            completion->set_value(CookieListResult{false, {}, "cookie manager is not available"});
            return;
        }
        auto *holder = new std::shared_ptr<std::promise<CookieListResult>>(completion);
        webkit_cookie_manager_get_cookies(
            manager,
            native_uri.c_str(),
            nullptr,
            [](GObject *object, GAsyncResult *result, gpointer user_data) {
                auto *holder = static_cast<std::shared_ptr<std::promise<CookieListResult>> *>(user_data);
                GError *error = nullptr;
                GList *cookies = webkit_cookie_manager_get_cookies_finish(
                    WEBKIT_COOKIE_MANAGER(object),
                    result,
                    &error
                );
                if (error) {
                    std::string message = error->message ? error->message : "get cookies failed";
                    g_error_free(error);
                    LOGGER_E("linux.cookie.allCookies.callback: get cookies failed error=%s", message.c_str());
                    (*holder)->set_value(CookieListResult{false, {}, message});
                    delete holder;
                    return;
                }
                wvbridge::CookiePropertiesList properties;
                for (GList *item = cookies; item; item = item->next) {
                    LOGGER_V("linux.cookie.allCookies.callback: converting cookie item=%p", item->data);
                    properties.push_back(PLATFORM_COOKIE_TO_PROPERTIES(
                        static_cast<SoupCookie *>(item->data)
                    ));
                }
                if (cookies) g_list_free_full(cookies, reinterpret_cast<GDestroyNotify>(soup_cookie_free));
                (*holder)->set_value(CookieListResult{true, std::move(properties), ""});
                LOGGER_D("linux.cookie.allCookies.callback: exit success");
                delete holder;
            },
            holder
        );
    });
    if (!dispatched) {
        LOGGER_E("linux.cookie.allCookies: GTK dispatch failed");
        throw_jni_exception(env, "java/lang/IllegalStateException", "GTK runtime is not available");
        return nullptr;
    }
    CookieListResult result = future.get();
    if (!result.ok) {
        LOGGER_E("linux.cookie.allCookies: platform operation failed error=%s", result.error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return nullptr;
    }
    LOGGER_I("linux.cookie.allCookies: exit success count=%zu", result.cookies.size());
    return wvbridge::new_structed_cookie_array(env, result.cookies);
}

#include "javascript-helpers.h"

#include <wvbridge/cookie.h>
#include <wvbridge/logger.h>

#include <atomic>
#include <cctype>
#include <future>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

struct CookieListCompletion {
    std::promise<std::pair<HRESULT, wvbridge::CookiePropertiesList>> promise;
    std::atomic_bool completed{false};

    void complete(HRESULT hr, wvbridge::CookiePropertiesList cookies = {}) {
        LOGGER_D("windows.cookie.listCompletion: complete hr=0x%08lx count=%zu already_completed=%d", (unsigned long) hr, cookies.size(), completed.load());
        if (!completed.exchange(true)) promise.set_value({hr, std::move(cookies)});
    }
};

std::string upper_ascii(std::string value) {
    for (char &ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

HRESULT get_cookie_manager(WebViewContext *ctx, ComPtr<ICoreWebView2CookieManager> *out) {
    LOGGER_V("windows.cookie.getCookieManager: enter ctx=%p webview=%p out=%p", ctx, ctx ? ctx->webview.Get() : nullptr, out);
    if (!ctx || !ctx->webview || !out) {
        LOGGER_E("windows.cookie.getCookieManager: exit E_POINTER because context/webview/out unavailable");
        return E_POINTER;
    }
    ComPtr<ICoreWebView2_2> webview2;
    HRESULT hr = ctx->webview.As(&webview2);
    if (FAILED(hr) || !webview2) {
        LOGGER_E("windows.cookie.getCookieManager: exit failed querying ICoreWebView2_2 hr=0x%08lx webview2=%p", (unsigned long) hr, webview2.Get());
        return FAILED(hr) ? hr : E_NOINTERFACE;
    }
    hr = webview2->get_CookieManager(out->ReleaseAndGetAddressOf());
    LOGGER_D("windows.cookie.getCookieManager: exit hr=0x%08lx manager=%p", (unsigned long) hr, out->Get());
    return hr;
}

HRESULT create_platform_cookie(
    ICoreWebView2CookieManager *manager,
    const wvbridge::CookieProperties &properties,
    const wvbridge::CookieUri &origin,
    ComPtr<ICoreWebView2Cookie> *out
) {
    LOGGER_I("windows.cookie.createPlatformCookie: enter manager=%p property_count=%zu origin=%s%s", manager, properties.size(), origin.host.c_str(), origin.path.c_str());
    if (!manager || !out) return E_POINTER;
    const std::wstring name = utf8_to_wstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_NAME).c_str()
    );
    const std::wstring value = utf8_to_wstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_VALUE).c_str()
    );
    const std::wstring domain = utf8_to_wstring(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_DOMAIN, origin.host).c_str()
    );
    const std::wstring path = utf8_to_wstring(
        wvbridge::cookie_property(
            properties,
            wvbridge::COOKIE_PATH,
            wvbridge::default_cookie_path(origin.path)
        ).c_str()
    );

    HRESULT hr = manager->CreateCookie(
        name.c_str(),
        value.c_str(),
        domain.c_str(),
        path.c_str(),
        out->ReleaseAndGetAddressOf()
    );
    if (FAILED(hr) || !*out) {
        LOGGER_E("windows.cookie.createPlatformCookie: CreateCookie failed hr=0x%08lx cookie=%p", (unsigned long) hr, out->Get());
        return FAILED(hr) ? hr : E_FAIL;
    }

    hr = (*out)->put_IsHttpOnly(
        wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_HTTP_ONLY) ? TRUE : FALSE
    );
    if (FAILED(hr)) { LOGGER_E("windows.cookie.createPlatformCookie: put_IsHttpOnly failed hr=0x%08lx", (unsigned long) hr); return hr; }
    hr = (*out)->put_IsSecure(
        wvbridge::cookie_property_bool(properties, wvbridge::COOKIE_SECURE) ? TRUE : FALSE
    );
    if (FAILED(hr)) { LOGGER_E("windows.cookie.createPlatformCookie: put_IsSecure failed hr=0x%08lx", (unsigned long) hr); return hr; }

    const std::string same_site = upper_ascii(
        wvbridge::cookie_property(properties, wvbridge::COOKIE_SAME_SITE)
    );
    if (!same_site.empty()) {
        COREWEBVIEW2_COOKIE_SAME_SITE_KIND kind = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX;
        if (same_site == "NONE") kind = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE;
        else if (same_site == "STRICT") kind = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_STRICT;
        hr = (*out)->put_SameSite(kind);
        if (FAILED(hr)) { LOGGER_E("windows.cookie.createPlatformCookie: put_SameSite failed hr=0x%08lx same_site=%s", (unsigned long) hr, same_site.c_str()); return hr; }
    }

    const std::string expires = wvbridge::cookie_property(properties, wvbridge::COOKIE_EXPIRES);
    const bool session = wvbridge::cookie_property_bool(
        properties,
        wvbridge::COOKIE_SESSION,
        expires.empty()
    );
    if (!session && !expires.empty()) {
        try {
            hr = (*out)->put_Expires(static_cast<double>(std::stoll(expires)) / 1000.0);
        } catch (...) {
            LOGGER_E("windows.cookie.createPlatformCookie: expires parse failed value=%s", expires.c_str());
            return E_INVALIDARG;
        }
    }
    LOGGER_D("windows.cookie.createPlatformCookie: exit hr=0x%08lx session=%d same_site=%s", (unsigned long) hr, session, same_site.c_str());
    return hr;
}

HRESULT platform_cookie_to_properties(
    ICoreWebView2Cookie *cookie,
    wvbridge::CookieProperties *out
) {
    LOGGER_V("windows.cookie.platformToProperties: enter cookie=%p out=%p", cookie, out);
    if (!cookie || !out) return E_POINTER;
    LPWSTR name = nullptr;
    LPWSTR value = nullptr;
    LPWSTR domain = nullptr;
    LPWSTR path = nullptr;
    HRESULT hr = cookie->get_Name(&name);
    if (SUCCEEDED(hr)) hr = cookie->get_Value(&value);
    if (SUCCEEDED(hr)) hr = cookie->get_Domain(&domain);
    if (SUCCEEDED(hr)) hr = cookie->get_Path(&path);
    if (FAILED(hr)) {
        if (name) CoTaskMemFree(name);
        if (value) CoTaskMemFree(value);
        if (domain) CoTaskMemFree(domain);
        if (path) CoTaskMemFree(path);
        LOGGER_E("windows.cookie.platformToProperties: basic property read failed hr=0x%08lx", (unsigned long) hr);
        return hr;
    }

    (*out)[wvbridge::COOKIE_NAME] = wstring_to_utf8_local(name ? name : L"");
    (*out)[wvbridge::COOKIE_VALUE] = wstring_to_utf8_local(value ? value : L"");
    (*out)[wvbridge::COOKIE_DOMAIN] = wstring_to_utf8_local(domain ? domain : L"");
    (*out)[wvbridge::COOKIE_PATH] = wstring_to_utf8_local(path ? path : L"/");
    CoTaskMemFree(name);
    CoTaskMemFree(value);
    CoTaskMemFree(domain);
    CoTaskMemFree(path);

    BOOL http_only = FALSE;
    BOOL secure = FALSE;
    BOOL session = FALSE;
    COREWEBVIEW2_COOKIE_SAME_SITE_KIND same_site = COREWEBVIEW2_COOKIE_SAME_SITE_KIND_LAX;
    double expires = -1.0;
    if (SUCCEEDED(hr)) hr = cookie->get_IsHttpOnly(&http_only);
    if (SUCCEEDED(hr)) hr = cookie->get_IsSecure(&secure);
    if (SUCCEEDED(hr)) hr = cookie->get_IsSession(&session);
    if (SUCCEEDED(hr)) hr = cookie->get_SameSite(&same_site);
    if (SUCCEEDED(hr)) hr = cookie->get_Expires(&expires);
    if (FAILED(hr)) { LOGGER_E("windows.cookie.platformToProperties: metadata read failed hr=0x%08lx", (unsigned long) hr); return hr; }

    (*out)[wvbridge::COOKIE_HTTP_ONLY] = http_only ? "true" : "false";
    (*out)[wvbridge::COOKIE_SECURE] = secure ? "true" : "false";
    (*out)[wvbridge::COOKIE_SESSION] = session ? "true" : "false";
    (*out)[wvbridge::COOKIE_SAME_SITE] =
        same_site == COREWEBVIEW2_COOKIE_SAME_SITE_KIND_NONE ? "NONE" :
        same_site == COREWEBVIEW2_COOKIE_SAME_SITE_KIND_STRICT ? "STRICT" : "LAX";
    if (!session) {
        (*out)[wvbridge::COOKIE_EXPIRES] = std::to_string(static_cast<long long>(expires * 1000.0));
    }
    LOGGER_D("windows.cookie.platformToProperties: exit name=%s domain=%s session=%d", (*out)[wvbridge::COOKIE_NAME].c_str(), (*out)[wvbridge::COOKIE_DOMAIN].c_str(), session);
    return S_OK;
}

#define PLATFORM_COOKIE_TO_PROPERTIES(cookie, out) platform_cookie_to_properties((cookie), (out))
#define JNI_TO_PLATFORM_COOKIE(manager, properties, origin, out) \
    create_platform_cookie((manager), (properties), (origin), (out))

bool read_uri(JNIEnv *env, jstring uri, std::wstring *native_uri, wvbridge::CookieUri *origin) {
    LOGGER_I("windows.cookie.readUri: enter env=%p uri=%p", env, uri);
    if (!uri) {
        LOGGER_E("windows.cookie.readUri: uri is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "uri is null");
        return false;
    }
    *native_uri = jstring_to_wstring(env, uri);
    if (env->ExceptionCheck()) { LOGGER_E("windows.cookie.readUri: string conversion raised exception"); return false; }
    const std::string utf8 = wstring_to_utf8_local(*native_uri);
    std::string error;
    if (!wvbridge::parse_cookie_uri(utf8, origin, &error)) {
        LOGGER_E("windows.cookie.readUri: invalid uri=%s error=%s", utf8.c_str(), error.c_str());
        throw_jni_exception(env, "java/lang/IllegalArgumentException", error.c_str());
        return false;
    }
    LOGGER_D("windows.cookie.readUri: exit true host=%s path=%s", origin->host.c_str(), origin->path.c_str());
    return true;
}

} // namespace

API_EXPORT(void, putCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("windows.cookie.putCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    wvbridge::CookieProperties properties;
    if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) { LOGGER_E("windows.cookie.putCookie: Java cookie conversion failed"); return; }
    std::wstring native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("windows.cookie.putCookie: URI validation failed"); return; }
    auto *ctx = require_context(env, handle);
    if (!ctx) { LOGGER_E("windows.cookie.putCookie: context unavailable"); return; }

    HRESULT result = E_FAIL;
    webview2_thread_run_sync(ctx->thread, [&] {
        LOGGER_V("windows.cookie.putCookie.webview: enter ctx=%p", ctx);
        ComPtr<ICoreWebView2CookieManager> manager;
        result = get_cookie_manager(ctx, &manager);
        if (FAILED(result)) { LOGGER_E("windows.cookie.putCookie.webview: get cookie manager failed hr=0x%08lx", (unsigned long) result); return; }
        ComPtr<ICoreWebView2Cookie> native_cookie;
        result = JNI_TO_PLATFORM_COOKIE(manager.Get(), properties, origin, &native_cookie);
        if (SUCCEEDED(result)) result = manager->AddOrUpdateCookie(native_cookie.Get());
    });
    if (FAILED(result)) { LOGGER_E("windows.cookie.putCookie: exit failure hr=0x%08lx", (unsigned long) result); throw_hresult(env, "putCookie", result); return; }
    LOGGER_I("windows.cookie.putCookie: exit success");
}

API_EXPORT(void, removeCookie, jlong handle, jstring uri, jobject cookie) {
    LOGGER_I("windows.cookie.removeCookie: enter handle=%lld uri=%p cookie=%p", (long long) handle, uri, cookie);
    wvbridge::CookieProperties properties;
    if (!wvbridge::java_cookie_to_properties(env, cookie, &properties)) { LOGGER_E("windows.cookie.removeCookie: Java cookie conversion failed"); return; }
    std::wstring native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("windows.cookie.removeCookie: URI validation failed"); return; }
    auto *ctx = require_context(env, handle);
    if (!ctx) { LOGGER_E("windows.cookie.removeCookie: context unavailable"); return; }

    HRESULT result = E_FAIL;
    webview2_thread_run_sync(ctx->thread, [&] {
        LOGGER_V("windows.cookie.removeCookie.webview: enter ctx=%p", ctx);
        ComPtr<ICoreWebView2CookieManager> manager;
        result = get_cookie_manager(ctx, &manager);
        if (FAILED(result)) { LOGGER_E("windows.cookie.removeCookie.webview: get cookie manager failed hr=0x%08lx", (unsigned long) result); return; }
        ComPtr<ICoreWebView2Cookie> native_cookie;
        result = JNI_TO_PLATFORM_COOKIE(manager.Get(), properties, origin, &native_cookie);
        if (SUCCEEDED(result)) result = manager->DeleteCookie(native_cookie.Get());
    });
    if (FAILED(result)) { LOGGER_E("windows.cookie.removeCookie: exit failure hr=0x%08lx", (unsigned long) result); throw_hresult(env, "removeCookie", result); return; }
    LOGGER_I("windows.cookie.removeCookie: exit success");
}

API_EXPORT(void, clearAllCookies, jlong handle) {
    LOGGER_I("windows.cookie.clearAllCookies: enter handle=%lld", (long long) handle);
    auto *ctx = require_context(env, handle);
    if (!ctx) { LOGGER_E("windows.cookie.clearAllCookies: context unavailable"); return; }
    HRESULT result = E_FAIL;
    webview2_thread_run_sync(ctx->thread, [&] {
        LOGGER_V("windows.cookie.clearAllCookies.webview: enter ctx=%p", ctx);
        ComPtr<ICoreWebView2CookieManager> manager;
        result = get_cookie_manager(ctx, &manager);
        if (SUCCEEDED(result)) result = manager->DeleteAllCookies();
    });
    if (FAILED(result)) { LOGGER_E("windows.cookie.clearAllCookies: exit failure hr=0x%08lx", (unsigned long) result); throw_hresult(env, "clearAllCookies", result); return; }
    LOGGER_I("windows.cookie.clearAllCookies: exit success");
}

API_EXPORT(jobjectArray, allCookies, jlong handle, jstring uri) {
    LOGGER_I("windows.cookie.allCookies: enter handle=%lld uri=%p", (long long) handle, uri);
    std::wstring native_uri;
    wvbridge::CookieUri origin;
    if (!read_uri(env, uri, &native_uri, &origin)) { LOGGER_E("windows.cookie.allCookies: URI validation failed"); return nullptr; }
    auto *ctx = require_context(env, handle);
    if (!ctx) { LOGGER_E("windows.cookie.allCookies: context unavailable"); return nullptr; }

    auto completion = std::make_shared<CookieListCompletion>();
    auto future = completion->promise.get_future();
    webview2_thread_run_sync(ctx->thread, [ctx, native_uri, completion] {
        LOGGER_V("windows.cookie.allCookies.webview: enter ctx=%p", ctx);
        ComPtr<ICoreWebView2CookieManager> manager;
        HRESULT hr = get_cookie_manager(ctx, &manager);
        if (FAILED(hr)) {
            LOGGER_E("windows.cookie.allCookies.webview: get cookie manager failed hr=0x%08lx", (unsigned long) hr);
            completion->complete(hr);
            return;
        }
        auto callback = Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [completion](HRESULT error_code, ICoreWebView2CookieList *list) -> HRESULT {
                if (FAILED(error_code) || !list) {
                    LOGGER_E("windows.cookie.allCookies.callback: failed error=0x%08lx list=%p", (unsigned long) error_code, list);
                    completion->complete(FAILED(error_code) ? error_code : E_FAIL);
                    return S_OK;
                }
                UINT32 count = 0;
                HRESULT hr = list->get_Count(&count);
                wvbridge::CookiePropertiesList cookies;
                for (UINT32 index = 0; SUCCEEDED(hr) && index < count; ++index) {
                    ComPtr<ICoreWebView2Cookie> cookie;
                    hr = list->GetValueAtIndex(index, cookie.GetAddressOf());
                    if (FAILED(hr) || !cookie) { LOGGER_E("windows.cookie.allCookies.callback: GetValueAtIndex failed index=%u hr=0x%08lx cookie=%p", index, (unsigned long) hr, cookie.Get()); break; }
                    wvbridge::CookieProperties properties;
                    hr = PLATFORM_COOKIE_TO_PROPERTIES(cookie.Get(), &properties);
                    if (SUCCEEDED(hr)) cookies.push_back(std::move(properties));
                }
                const size_t cookie_count = cookies.size();
                completion->complete(hr, std::move(cookies));
                LOGGER_D("windows.cookie.allCookies.callback: exit hr=0x%08lx count=%zu", (unsigned long) hr, cookie_count);
                return S_OK;
            }
        );
        hr = manager->GetCookies(native_uri.c_str(), callback.Get());
        if (FAILED(hr)) { LOGGER_E("windows.cookie.allCookies.webview: GetCookies dispatch failed hr=0x%08lx", (unsigned long) hr); completion->complete(hr); }
    });

    auto result = future.get();
    if (FAILED(result.first)) {
        LOGGER_E("windows.cookie.allCookies: exit failure hr=0x%08lx", (unsigned long) result.first);
        throw_hresult(env, "allCookies", result.first);
        return nullptr;
    }
    LOGGER_I("windows.cookie.allCookies: exit success count=%zu", result.second.size());
    return wvbridge::new_structed_cookie_array(env, result.second);
}

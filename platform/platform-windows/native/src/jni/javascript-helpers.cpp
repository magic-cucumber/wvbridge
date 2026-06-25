#include "javascript-helpers.h"

#include <cwchar>
#include <sstream>

std::wstring jstring_to_wstring(JNIEnv *env, jstring value) {
    LOGGER_V("jstring_to_wstring: value=%p", value);
    if (!value) return L"";
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        LOGGER_V("jstring_to_wstring: GetStringUTFChars returned null");
        return L"";
    }
    std::wstring result = utf8_to_wstring(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string wstring_to_utf8_local(const std::wstring &value) {
    LOGGER_V("wstring_to_utf8_local: value length=%zu", value.size());
    if (value.empty()) return "";

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        LOGGER_V("wstring_to_utf8_local: WideCharToMultiByte size query failed, required=%d", required);
        return "";
    }

    std::string out(static_cast<size_t>(required), '\0');
    int converted = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), required, nullptr, nullptr);
    if (converted <= 0) {
        LOGGER_V("wstring_to_utf8_local: WideCharToMultiByte conversion failed, converted=%d", converted);
        return "";
    }
    return out;
}

WebViewContext *require_context(JNIEnv *env, jlong handle) {
    LOGGER_V("require_context: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("require_context: handle is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(static_cast<uintptr_t>(handle));
    if (!ctx || !ctx->webview) {
        LOGGER_E("require_context: webview is not available, JNI exception will be set");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return nullptr;
    }
    LOGGER_V("require_context: returning context=%p", ctx);
    return ctx;
}

void throw_hresult(JNIEnv *env, const char *operation, HRESULT hr) {
    LOGGER_V("throw_hresult: operation=%s hr=0x%lx", operation, (unsigned long)hr);
    std::ostringstream oss;
    oss << operation << " failed [HRESULT=" << format_hresult(hr) << "]";
    throw_jni_exception(env, "java/lang/RuntimeException", oss.str().c_str());
}

HRESULT ensure_web_message_registered(WebViewContext *ctx) {
    if (!ctx || !ctx->webview) return E_FAIL;
    if (ctx->web_message_received_registered) return S_OK;

    auto callback = Callback<ICoreWebView2WebMessageReceivedEventHandler>(
        [ctx](ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *args) -> HRESULT {
            LOGGER_V("ensure_web_message_registered: WebMessageReceived callback ctx=%p args=%p", ctx, args);
            if (!args) {
                LOGGER_V("ensure_web_message_registered: WebMessageReceived args is null, ignoring");
                return S_OK;
            }

            LPWSTR message = nullptr;
            LOGGER_V("ensure_web_message_registered: calling TryGetWebMessageAsString");
            HRESULT hr = args->TryGetWebMessageAsString(&message);
            LOGGER_V(
                "ensure_web_message_registered: TryGetWebMessageAsString hr=0x%lx message=%p",
                (unsigned long)hr,
                message
            );
            if (SUCCEEDED(hr) && message) {
                LOGGER_V(
                    "ensure_web_message_registered: dispatching string message len=%zu preview=%.*ls",
                    wcslen(message),
                    100,
                    message
                );
                wvbridge::dispatch_web_message_to_java(
                    ctx->web_message_handlers_mutex,
                    ctx->web_message_handlers,
                    message
                );
                LOGGER_V("ensure_web_message_registered: string message dispatched, freeing buffer");
                CoTaskMemFree(message);
                return S_OK;
            }
            if (message) {
                LOGGER_V("ensure_web_message_registered: freeing unused string buffer after hr=0x%lx", (unsigned long)hr);
                CoTaskMemFree(message);
            } else {
                LOGGER_V("ensure_web_message_registered: no string payload available, falling back to JSON");
            }

            LPWSTR json = nullptr;
            LOGGER_V("ensure_web_message_registered: calling get_WebMessageAsJson");
            hr = args->get_WebMessageAsJson(&json);
            LOGGER_V(
                "ensure_web_message_registered: get_WebMessageAsJson hr=0x%lx json=%p",
                (unsigned long)hr,
                json
            );
            if (SUCCEEDED(hr) && json) {
                LOGGER_V(
                    "ensure_web_message_registered: dispatching JSON message len=%zu preview=%.*ls",
                    wcslen(json),
                    100,
                    json
                );
                wvbridge::dispatch_web_message_to_java(
                    ctx->web_message_handlers_mutex,
                    ctx->web_message_handlers,
                    json
                );
                LOGGER_V("ensure_web_message_registered: JSON message dispatched, freeing buffer");
                CoTaskMemFree(json);
            } else if (json) {
                LOGGER_V("ensure_web_message_registered: freeing JSON buffer after failed hr=0x%lx", (unsigned long)hr);
                CoTaskMemFree(json);
            } else {
                LOGGER_V("ensure_web_message_registered: JSON payload unavailable, hr=0x%lx", (unsigned long)hr);
            }
            LOGGER_V("ensure_web_message_registered: WebMessageReceived callback complete");
            return S_OK;
        }
    );

    HRESULT hr = ctx->webview->add_WebMessageReceived(callback.Get(), &ctx->web_message_received_token);
    if (SUCCEEDED(hr)) {
        ctx->web_message_received_registered = true;
    }
    return hr;
}

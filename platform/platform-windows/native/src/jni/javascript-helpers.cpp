#include "javascript-helpers.h"

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
            if (!args) return S_OK;

            LPWSTR message = nullptr;
            HRESULT hr = args->TryGetWebMessageAsString(&message);
            if (SUCCEEDED(hr) && message) {
                wvbridge::dispatch_web_message_to_java(
                    ctx->web_message_handlers_mutex,
                    ctx->web_message_handlers,
                    message
                );
                CoTaskMemFree(message);
                return S_OK;
            }
            if (message) CoTaskMemFree(message);

            LPWSTR json = nullptr;
            hr = args->get_WebMessageAsJson(&json);
            if (SUCCEEDED(hr) && json) {
                wvbridge::dispatch_web_message_to_java(
                    ctx->web_message_handlers_mutex,
                    ctx->web_message_handlers,
                    json
                );
                CoTaskMemFree(json);
            } else if (json) {
                CoTaskMemFree(json);
            }
            return S_OK;
        }
    );

    HRESULT hr = ctx->webview->add_WebMessageReceived(callback.Get(), &ctx->web_message_received_token);
    if (SUCCEEDED(hr)) {
        ctx->web_message_received_registered = true;
    }
    return hr;
}

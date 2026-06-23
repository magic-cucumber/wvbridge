#include "libs_helpers.h"

#include <future>

namespace {

constexpr const wchar_t *UNDEFINED_SENTINEL = L"__wvbridge_native_undefined_result__";

std::wstring jstring_to_wstring(JNIEnv *env, jstring value) {
    if (!value) return L"";
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return L"";
    std::wstring result = utf8_to_wstring(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

std::string wstring_to_utf8_local(const std::wstring &value) {
    if (value.empty()) return "";

    int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return "";

    std::string out(static_cast<size_t>(required), '\0');
    int converted = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), required, nullptr, nullptr);
    if (converted <= 0) return "";
    return out;
}

std::wstring js_string_literal(const std::wstring &value) {
    std::wostringstream out;
    out << L'"';
    for (wchar_t ch: value) {
        switch (ch) {
            case L'\\':
                out << L"\\\\";
                break;
            case L'"':
                out << L"\\\"";
                break;
            case L'\b':
                out << L"\\b";
                break;
            case L'\f':
                out << L"\\f";
                break;
            case L'\n':
                out << L"\\n";
                break;
            case L'\r':
                out << L"\\r";
                break;
            case L'\t':
                out << L"\\t";
                break;
            default:
                if (ch < 0x20) {
                    wchar_t buffer[7];
                    swprintf_s(buffer, L"\\u%04X", static_cast<unsigned int>(ch));
                    out << buffer;
                } else {
                    out << ch;
                }
        }
    }
    out << L'"';
    return out.str();
}

std::wstring build_evaluate_script_source(const std::wstring &source) {
    return L"(() => { const __wvbridge_result = (0, eval)("
        + js_string_literal(source)
        + L"); return __wvbridge_result === undefined ? "
        + js_string_literal(UNDEFINED_SENTINEL)
        + L" : __wvbridge_result; })()";
}

WebViewContext *require_context(JNIEnv *env, jlong handle) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(static_cast<uintptr_t>(handle));
    if (!ctx || !ctx->webview) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return nullptr;
    }
    return ctx;
}

void throw_hresult(JNIEnv *env, const char *operation, HRESULT hr) {
    std::ostringstream oss;
    oss << operation << " failed [HRESULT=" << format_hresult(hr) << "]";
    throw_jni_exception(env, "java/lang/RuntimeException", oss.str().c_str());
}

}

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return nullptr;
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    const std::wstring source = jstring_to_wstring(env, script);
    if (env->ExceptionCheck()) return nullptr;

    struct Result {
        HRESULT hr = E_FAIL;
        bool has_value = false;
        std::wstring value;
    };

    const std::wstring executableSource = build_evaluate_script_source(source);
    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    webview2_thread_run_sync(ctx->thread, [ctx, executableSource, completion] {
        if (!ctx->webview) {
            completion->set_value(Result{E_FAIL, false, L""});
            return;
        }

        auto callback = Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [completion](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                completion->set_value(Result{errorCode, resultObjectAsJson != nullptr, resultObjectAsJson ? resultObjectAsJson : L""});
                return S_OK;
            }
        );

        HRESULT hr = ctx->webview->ExecuteScript(executableSource.c_str(), callback.Get());
        if (FAILED(hr)) {
            completion->set_value(Result{hr, false, L""});
        }
    });

    Result result = future.get();
    if (FAILED(result.hr)) {
        throw_hresult(env, "ExecuteScript", result.hr);
        return nullptr;
    }

    if (!result.has_value) return nullptr;
    if (result.value == js_string_literal(UNDEFINED_SENTINEL)) return nullptr;
    const std::string utf8 = wstring_to_utf8_local(result.value);
    return env->NewStringUTF(utf8.c_str());
}

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    const std::wstring source = jstring_to_wstring(env, script);
    if (env->ExceptionCheck()) return 0;

    struct Result {
        HRESULT hr = E_FAIL;
        jlong hook_id = 0;
    };

    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    webview2_thread_run_sync(ctx->thread, [ctx, source, completion] {
        if (!ctx->webview) {
            completion->set_value(Result{E_FAIL, 0});
            return;
        }

        const jlong hookId = ctx->next_document_start_hook_id++;
        auto callback = Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [ctx, completion, hookId](HRESULT errorCode, LPCWSTR id) -> HRESULT {
                if (SUCCEEDED(errorCode) && id) {
                    ctx->document_start_hook_ids[hookId] = id;
                }
                completion->set_value(Result{errorCode, hookId});
                return S_OK;
            }
        );

        HRESULT hr = ctx->webview->AddScriptToExecuteOnDocumentCreated(source.c_str(), callback.Get());
        if (FAILED(hr)) {
            completion->set_value(Result{hr, 0});
        }
    });

    Result result = future.get();
    if (FAILED(result.hr)) {
        throw_hresult(env, "AddScriptToExecuteOnDocumentCreated", result.hr);
        return 0;
    }
    return result.hook_id;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    HRESULT hr = S_OK;
    webview2_thread_run_sync(ctx->thread, [ctx, hookId, &hr] {
        auto it = ctx->document_start_hook_ids.find(hookId);
        if (it == ctx->document_start_hook_ids.end()) return;
        if (!ctx->webview) {
            hr = E_FAIL;
            return;
        }

        hr = ctx->webview->RemoveScriptToExecuteOnDocumentCreated(it->second.c_str());
        if (SUCCEEDED(hr)) {
            ctx->document_start_hook_ids.erase(it);
        }
    });

    if (FAILED(hr)) {
        throw_hresult(env, "RemoveScriptToExecuteOnDocumentCreated", hr);
    }
}

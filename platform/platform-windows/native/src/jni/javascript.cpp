#include "libs_helpers.h"
#include <wvbridge/logger.h>

#include <future>

namespace {

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

}

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    LOGGER_I("evaluateScript: handle=%lld script=%p", (long long)handle, script);
    auto *ctx = require_context(env, handle);
    if (!ctx) return nullptr;
    if (script == nullptr) {
        LOGGER_E("evaluateScript: script is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    const std::wstring source = jstring_to_wstring(env, script);
    if (env->ExceptionCheck()) return nullptr;
    LOGGER_V("evaluateScript: script length=%zu", source.size());

    struct Result {
        HRESULT hr = E_FAIL;
        bool has_value = false;
        std::wstring value;
    };

    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    LOGGER_V("evaluateScript: dispatching ExecuteScript to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, source, completion] {
        if (!ctx->webview) {
            LOGGER_V("evaluateScript: webview is null in dispatch");
            completion->set_value(Result{E_FAIL, false, L""});
            return;
        }

        auto callback = Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [completion](HRESULT errorCode, LPCWSTR resultObjectAsJson) -> HRESULT {
                completion->set_value(Result{errorCode, resultObjectAsJson != nullptr, resultObjectAsJson ? resultObjectAsJson : L""});
                return S_OK;
            }
        );

        HRESULT hr = ctx->webview->ExecuteScript(source.c_str(), callback.Get());
        if (FAILED(hr)) {
            LOGGER_V("evaluateScript: ExecuteScript failed, hr=0x%lx", (unsigned long)hr);
            completion->set_value(Result{hr, false, L""});
        }
    });

    Result result = future.get();
    if (FAILED(result.hr)) {
        LOGGER_E("evaluateScript: ExecuteScript async failed, hr=0x%lx", (unsigned long)result.hr);
        throw_hresult(env, "ExecuteScript", result.hr);
        return nullptr;
    }

    if (!result.has_value) {
        LOGGER_V("evaluateScript: result has no value, returning null");
        return nullptr;
    }
    const std::string utf8 = wstring_to_utf8_local(result.value);
    LOGGER_V("evaluateScript: returning result length=%zu", utf8.size());
    return env->NewStringUTF(utf8.c_str());
}

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    LOGGER_I("registerDocumentStartHook: handle=%lld script=%p", (long long)handle, script);
    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (script == nullptr) {
        LOGGER_E("registerDocumentStartHook: script is null, JNI exception will be set");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    const std::wstring source = jstring_to_wstring(env, script);
    if (env->ExceptionCheck()) return 0;
    LOGGER_V("registerDocumentStartHook: script length=%zu", source.size());

    struct Result {
        HRESULT hr = E_FAIL;
        jlong hook_id = 0;
    };

    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    LOGGER_V("registerDocumentStartHook: dispatching AddScriptToExecuteOnDocumentCreated to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, source, completion] {
        if (!ctx->webview) {
            LOGGER_V("registerDocumentStartHook: webview is null in dispatch");
            completion->set_value(Result{E_FAIL, 0});
            return;
        }

        const jlong hookId = ctx->next_document_start_hook_id++;
        LOGGER_V("registerDocumentStartHook: assigned hookId=%lld", (long long)hookId);
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
            LOGGER_V("registerDocumentStartHook: AddScriptToExecuteOnDocumentCreated failed, hr=0x%lx", (unsigned long)hr);
            completion->set_value(Result{hr, 0});
        }
    });

    Result result = future.get();
    if (FAILED(result.hr)) {
        LOGGER_E("registerDocumentStartHook: async operation failed, hr=0x%lx", (unsigned long)result.hr);
        throw_hresult(env, "AddScriptToExecuteOnDocumentCreated", result.hr);
        return 0;
    }
    LOGGER_V("registerDocumentStartHook: success, hook_id=%lld", (long long)result.hook_id);
    return result.hook_id;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    LOGGER_I("unregisterDocumentStartHook: handle=%lld hookId=%lld", (long long)handle, (long long)hookId);
    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    HRESULT hr = S_OK;
    LOGGER_V("unregisterDocumentStartHook: dispatching RemoveScriptToExecuteOnDocumentCreated to webview thread");
    webview2_thread_run_sync(ctx->thread, [ctx, hookId, &hr] {
        auto it = ctx->document_start_hook_ids.find(hookId);
        if (it == ctx->document_start_hook_ids.end()) {
            LOGGER_V("unregisterDocumentStartHook: hookId=%lld not found", (long long)hookId);
            return;
        }
        if (!ctx->webview) {
            LOGGER_V("unregisterDocumentStartHook: webview is null, aborting");
            hr = E_FAIL;
            return;
        }

        LOGGER_V("unregisterDocumentStartHook: calling RemoveScriptToExecuteOnDocumentCreated");
        hr = ctx->webview->RemoveScriptToExecuteOnDocumentCreated(it->second.c_str());
        if (SUCCEEDED(hr)) {
            LOGGER_V("unregisterDocumentStartHook: successfully removed hookId=%lld", (long long)hookId);
            ctx->document_start_hook_ids.erase(it);
        } else {
            LOGGER_V("unregisterDocumentStartHook: RemoveScriptToExecuteOnDocumentCreated failed, hr=0x%lx", (unsigned long)hr);
        }
    });

    if (FAILED(hr)) {
        LOGGER_E("unregisterDocumentStartHook: operation failed, hr=0x%lx", (unsigned long)hr);
        throw_hresult(env, "RemoveScriptToExecuteOnDocumentCreated", hr);
    }
}

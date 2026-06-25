#include "javascript-helpers.h"

#include <future>

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

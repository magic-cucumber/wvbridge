#include "javascript-helpers.h"

#include <future>

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

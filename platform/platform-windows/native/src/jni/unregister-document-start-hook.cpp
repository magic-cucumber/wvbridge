#include "javascript-helpers.h"

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

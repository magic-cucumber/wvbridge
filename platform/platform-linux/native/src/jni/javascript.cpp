#include "libs_helpers.h"
#include <wvbridge/logger.h>

#include <future>

namespace {

std::string jstring_to_string(JNIEnv *env, jstring value) {
    LOGGER_V("jstring_to_string: converting jstring");
    if (!value) {
        LOGGER_V("jstring_to_string: value is null, returning empty");
        return "";
    }
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        LOGGER_V("jstring_to_string: GetStringUTFChars returned null");
        return "";
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    LOGGER_V("jstring_to_string: result len=%zu", result.size());
    return result;
}

WebViewContext *require_context(JNIEnv *env, jlong handle) {
    LOGGER_I("require_context: handle=%lld", (long long)handle);
    if (handle == 0) {
        LOGGER_E("require_context: null handle, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }

    auto *ctx = reinterpret_cast<WebViewContext *>(static_cast<uintptr_t>(handle));
    if (!ctx || !ctx->webview) {
        LOGGER_E("require_context: ctx=%p webview not available", (void*)ctx);
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return nullptr;
    }
    LOGGER_V("require_context: returning ctx=%p", (void*)ctx);
    return ctx;
}

WebKitUserScript *add_document_start_script(WebKitWebView *webview, const std::string &source) {
    LOGGER_V("add_document_start_script: source len=%zu", source.size());
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(webview);
    WebKitUserScript *script = webkit_user_script_new(
        source.c_str(),
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr,
        nullptr
    );
    webkit_user_content_manager_add_script(manager, script);
    LOGGER_V("add_document_start_script: script created=%p", (void*)script);
    return script;
}

}


API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    LOGGER_I("evaluateScript: handle=%lld", (long long)handle);

    auto *ctx = require_context(env, handle);
    if (!ctx) return nullptr;
    if (script == nullptr) {
        LOGGER_E("evaluateScript: null script, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    const std::string source = jstring_to_string(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_W("evaluateScript: JVM exception after jstring_to_string, aborting");
        return nullptr;
    }
    LOGGER_V("evaluateScript: source len=%zu", source.size());

    struct Result {
        bool ok = false;
        bool has_value = false;
        std::string value;
        std::string error;
    };

    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    LOGGER_V("evaluateScript: dispatching to GTK thread");
    wvbridge::gtk_run_on_thread_sync([ctx, source, completion] {
        if (!ctx->webview) {
            LOGGER_V("evaluateScript: ctx->webview is null in GTK thread");
            completion->set_value(Result{false, false, "", "webview is not available"});
            return;
        }

        LOGGER_V("evaluateScript: calling webkit_web_view_run_javascript");
        auto *completionPtr = new std::shared_ptr<std::promise<Result>>(completion);
        webkit_web_view_run_javascript(
            ctx->webview,
            source.c_str(),
            nullptr,
            [](GObject *object, GAsyncResult *asyncResult, gpointer userData) {
                auto completionHolder = static_cast<std::shared_ptr<std::promise<Result>> *>(userData);
                std::shared_ptr<std::promise<Result>> completion = *completionHolder;
                delete completionHolder;

                GError *error = nullptr;
                WebKitJavascriptResult *jsResult = webkit_web_view_run_javascript_finish(
                    WEBKIT_WEB_VIEW(object),
                    asyncResult,
                    &error
                );

                if (error) {
                    std::string message = error->message ? error->message : "WebKitGTK JavaScript evaluation failed";
                    LOGGER_V("evaluateScript: async callback error=%s", message.c_str());
                    g_error_free(error);
                    completion->set_value(Result{false, false, "", message});
                    return;
                }

                if (!jsResult) {
                    LOGGER_V("evaluateScript: async callback no jsResult");
                    completion->set_value(Result{true, false, "", ""});
                    return;
                }

                JSCValue *value = webkit_javascript_result_get_js_value(jsResult);
                if (!value || jsc_value_is_undefined(value)) {
                    LOGGER_V("evaluateScript: async callback value is undefined");
                    webkit_javascript_result_unref(jsResult);
                    completion->set_value(Result{true, false, "", ""});
                    return;
                }
                if (jsc_value_is_null(value)) {
                    LOGGER_V("evaluateScript: async callback value is null");
                    webkit_javascript_result_unref(jsResult);
                    completion->set_value(Result{true, true, "null", ""});
                    return;
                }

                gchar *stringValue = value ? jsc_value_to_string(value) : nullptr;
                std::string output = stringValue ? stringValue : "";
                if (stringValue) g_free(stringValue);
                webkit_javascript_result_unref(jsResult);

                LOGGER_V("evaluateScript: async callback result=%s", output.c_str());
                completion->set_value(Result{true, true, output, ""});
            },
            completionPtr
        );
    });

    Result result = future.get();
    if (!result.ok) {
        LOGGER_E("evaluateScript: failed, error=%s", result.error.c_str());
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return nullptr;
    }
    if (!result.has_value) {
        LOGGER_V("evaluateScript: no value returned");
        return nullptr;
    }
    LOGGER_V("evaluateScript: returning value len=%zu", result.value.size());
    return env->NewStringUTF(result.value.c_str());
}

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    LOGGER_I("registerDocumentStartHook: handle=%lld", (long long)handle);

    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (script == nullptr) {
        LOGGER_E("registerDocumentStartHook: null script, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    const std::string source = jstring_to_string(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_W("registerDocumentStartHook: JVM exception after jstring_to_string, aborting");
        return 0;
    }
    LOGGER_V("registerDocumentStartHook: source len=%zu", source.size());

    jlong hookId = 0;
    LOGGER_V("registerDocumentStartHook: dispatching to GTK thread");
    wvbridge::gtk_run_on_thread_sync([ctx, source, &hookId] {
        if (!ctx->webview) {
            LOGGER_V("registerDocumentStartHook: ctx->webview is null in GTK thread");
            return;
        }
        hookId = ctx->next_document_start_hook_id++;
        ctx->document_start_hooks[hookId] = add_document_start_script(ctx->webview, source);
        LOGGER_V("registerDocumentStartHook: hookId=%lld", (long long)hookId);
    });

    if (hookId == 0) {
        LOGGER_E("registerDocumentStartHook: hookId is 0, webview not available");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
    return hookId;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    LOGGER_I("unregisterDocumentStartHook: handle=%lld hookId=%lld", (long long)handle, (long long)hookId);

    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    bool webviewAvailable = true;
    LOGGER_V("unregisterDocumentStartHook: dispatching to GTK thread");
    wvbridge::gtk_run_on_thread_sync([ctx, hookId, &webviewAvailable] {
        if (!ctx->webview) {
            LOGGER_V("unregisterDocumentStartHook: ctx->webview is null in GTK thread");
            webviewAvailable = false;
            return;
        }

        auto it = ctx->document_start_hooks.find(hookId);
        if (it == ctx->document_start_hooks.end()) {
            LOGGER_V("unregisterDocumentStartHook: hookId=%lld not found", (long long)hookId);
            return;
        }

        WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
        webkit_user_content_manager_remove_script(manager, it->second);
        webkit_user_script_unref(it->second);
        ctx->document_start_hooks.erase(it);
        LOGGER_V("unregisterDocumentStartHook: hookId=%lld removed", (long long)hookId);
    });

    if (!webviewAvailable) {
        LOGGER_E("unregisterDocumentStartHook: webview not available");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}
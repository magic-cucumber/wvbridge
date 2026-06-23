#include "libs_helpers.h"

#include <future>

namespace {

std::string jstring_to_string(JNIEnv *env, jstring value) {
    if (!value) return "";
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return "";
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
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

WebKitUserScript *add_document_start_script(WebKitWebView *webview, const std::string &source) {
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(webview);
    WebKitUserScript *script = webkit_user_script_new(
        source.c_str(),
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nullptr,
        nullptr
    );
    webkit_user_content_manager_add_script(manager, script);
    return script;
}

}

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return nullptr;
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    const std::string source = jstring_to_string(env, script);
    if (env->ExceptionCheck()) return nullptr;

    struct Result {
        bool ok = false;
        std::string value;
        std::string error;
    };

    auto completion = std::make_shared<std::promise<Result>>();
    auto future = completion->get_future();

    wvbridge::gtk_run_on_thread_sync([ctx, source, completion] {
        if (!ctx->webview) {
            completion->set_value(Result{false, "", "webview is not available"});
            return;
        }

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
                    g_error_free(error);
                    completion->set_value(Result{false, "", message});
                    return;
                }

                if (!jsResult) {
                    completion->set_value(Result{true, "", ""});
                    return;
                }

                JSCValue *value = webkit_javascript_result_get_js_value(jsResult);
                gchar *stringValue = value ? jsc_value_to_string(value) : nullptr;
                std::string output = stringValue ? stringValue : "";
                if (stringValue) g_free(stringValue);
                webkit_javascript_result_unref(jsResult);

                completion->set_value(Result{true, output, ""});
            },
            completionPtr
        );
    });

    Result result = future.get();
    if (!result.ok) {
        throw_jni_exception(env, "java/lang/RuntimeException", result.error.c_str());
        return nullptr;
    }
    if (result.value.empty()) return nullptr;
    return env->NewStringUTF(result.value.c_str());
}

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return 0;
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    const std::string source = jstring_to_string(env, script);
    if (env->ExceptionCheck()) return 0;

    jlong hookId = 0;
    wvbridge::gtk_run_on_thread_sync([ctx, source, &hookId] {
        if (!ctx->webview) return;
        hookId = ctx->next_document_start_hook_id++;
        ctx->document_start_hooks[hookId] = add_document_start_script(ctx->webview, source);
    });

    if (hookId == 0) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
    return hookId;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    auto *ctx = require_context(env, handle);
    if (!ctx) return;

    bool webviewAvailable = true;
    wvbridge::gtk_run_on_thread_sync([ctx, hookId, &webviewAvailable] {
        if (!ctx->webview) {
            webviewAvailable = false;
            return;
        }

        auto it = ctx->document_start_hooks.find(hookId);
        if (it == ctx->document_start_hooks.end()) return;

        WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager(ctx->webview);
        webkit_user_content_manager_remove_script(manager, it->second);
        webkit_user_script_unref(it->second);
        ctx->document_start_hooks.erase(it);
    });

    if (!webviewAvailable) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

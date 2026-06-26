#include "javascript-helpers.h"

#include <future>

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

        LOGGER_V("evaluateScript: calling webkit_web_view_evaluate_javascript");
        auto *completionPtr = new std::shared_ptr<std::promise<Result>>(completion);
        webkit_web_view_evaluate_javascript(
            ctx->webview,
            source.c_str(),
            -1,
            nullptr,
            nullptr,
            nullptr,
            [](GObject *object, GAsyncResult *asyncResult, gpointer userData) {
                auto completionHolder = static_cast<std::shared_ptr<std::promise<Result>> *>(userData);
                std::shared_ptr<std::promise<Result>> completion = *completionHolder;
                delete completionHolder;

                GError *error = nullptr;
                JSCValue *value = webkit_web_view_evaluate_javascript_finish(
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

                if (!value) {
                    LOGGER_V("evaluateScript: async callback no value");
                    completion->set_value(Result{true, false, "", ""});
                    return;
                }

                if (!value || jsc_value_is_undefined(value)) {
                    LOGGER_V("evaluateScript: async callback value is undefined");
                    g_object_unref(value);
                    completion->set_value(Result{true, false, "", ""});
                    return;
                }
                if (jsc_value_is_null(value)) {
                    LOGGER_V("evaluateScript: async callback value is null");
                    g_object_unref(value);
                    completion->set_value(Result{true, true, "null", ""});
                    return;
                }
                if (jsc_value_is_boolean(value)) {
                    const bool booleanValue = jsc_value_to_boolean(value);
                    std::string output = booleanValue ? "true" : "false";
                    g_object_unref(value);

                    LOGGER_V("evaluateScript: async callback boolean result=%s", output.c_str());
                    completion->set_value(Result{true, true, output, ""});
                    return;
                }

                gchar *stringValue = value ? jsc_value_to_string(value) : nullptr;
                std::string output = stringValue ? stringValue : "";
                if (stringValue) g_free(stringValue);
                g_object_unref(value);

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

#include "javascript-helpers.h"

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

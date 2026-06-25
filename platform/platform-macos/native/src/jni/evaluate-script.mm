#import "javascript-helpers.h"

#include <cstring>

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    LOGGER_I("evaluateScript: handle=%lld", (long long) handle);
    if (script == nullptr) {
        LOGGER_W("evaluateScript: script is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    auto *ctx = require_context(env, handle, "evaluateScript");
    if (!ctx) return nullptr;

    NSString *source = jstring_to_nsstring(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_E("evaluateScript: JNI exception after string conversion");
        return nullptr;
    }
    LOGGER_V("evaluateScript: source length=%lu", (unsigned long) [source length]);

    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block id result = nil;
    __block NSError *error = nil;
    __block bool webViewAvailable = true;

    LOGGER_V("evaluateScript: dispatching evaluateJavaScript to main thread");
    runOnMainAsync(^{
        if (!ctx->webView) {
            LOGGER_E("evaluateScript: webView is not available in main block");
            webViewAvailable = false;
            dispatch_semaphore_signal(semaphore);
            return;
        }

        [ctx->webView evaluateJavaScript:source completionHandler:^(id value, NSError *evaluationError) {
            result = [value retain];
            error = [evaluationError retain];
            LOGGER_V("evaluateScript: JavaScript evaluation completed, result=%p error=%p", (void *) result, (void *) error);
            dispatch_semaphore_signal(semaphore);
        }];
    });

    LOGGER_V("evaluateScript: waiting for evaluation to complete");
    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);

    if (!webViewAvailable) {
        LOGGER_E("evaluateScript: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return nullptr;
    }
    if (error) {
        const char *errorMessage = [[error localizedDescription] UTF8String];
        LOGGER_E("evaluateScript: JavaScript error: %s", errorMessage ? errorMessage : "unknown");
        std::string message = errorMessage ? errorMessage : "WKWebView JavaScript evaluation failed";
        [error release];
        if (result) [result release];
        throw_jni_exception(env, "java/lang/RuntimeException", message.c_str());
        return nullptr;
    }
    if (!result) {
        LOGGER_V("evaluateScript: result is null, returning null");
        return nullptr;
    }

    NSString *stringValue = [result isKindOfClass:[NSString class]] ? (NSString *) result : [result description];
    const char *chars = [stringValue UTF8String];
    jstring output = env->NewStringUTF(chars ? chars : "");
    [result release];
    LOGGER_V("evaluateScript: returning string of length=%lu", (unsigned long) (chars ? strlen(chars) : 0));
    return output;
}

#include "libs_helpers.h"
#include <wvbridge/logger.h>

static NSString *jstring_to_nsstring(JNIEnv *env, jstring value) {
    if (!value) {
        LOGGER_V("jstring_to_nsstring: value is null");
        return nil;
    }
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        LOGGER_V("jstring_to_nsstring: GetStringUTFChars returned null");
        return nil;
    }
    NSString *result = [NSString stringWithUTF8String:chars] ?: @"";
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

static WKUserScript *create_document_start_script(NSString *source) {
    LOGGER_V("create_document_start_script: creating script");
    return [[WKUserScript alloc] initWithSource:source ?: @""
                                  injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                               forMainFrameOnly:NO];
}

static void add_document_start_script(WKWebView *webView, NSString *source) {
    LOGGER_V("add_document_start_script: adding script to webView=%p", (void *) webView);
    WKUserScript *userScript = create_document_start_script(source);
    [webView.configuration.userContentController addUserScript:userScript];
    [userScript release];
}

static void rebuild_document_start_scripts(WebViewContext *ctx) {
    LOGGER_V("rebuild_document_start_scripts: ctx=%p, removing all user scripts", (void *) ctx);
    [ctx->webView.configuration.userContentController removeAllUserScripts];
    for (NSNumber *hookId in ctx->documentStartHooks) {
        add_document_start_script(ctx->webView, ctx->documentStartHooks[hookId]);
    }
}

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    LOGGER_I("evaluateScript: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("evaluateScript: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }
    if (script == nullptr) {
        LOGGER_W("evaluateScript: script is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("evaluateScript: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("evaluateScript: ctx is null after cast, aborting");
        return nullptr;
    }

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

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    LOGGER_I("registerDocumentStartHook: handle=%lld", (long long) handle);
    if (handle == 0) {
        LOGGER_W("registerDocumentStartHook: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return 0;
    }
    if (script == nullptr) {
        LOGGER_W("registerDocumentStartHook: script is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("registerDocumentStartHook: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("registerDocumentStartHook: ctx is null after cast, aborting");
        return 0;
    }

    NSString *source = jstring_to_nsstring(env, script);
    if (env->ExceptionCheck()) {
        LOGGER_E("registerDocumentStartHook: JNI exception after string conversion");
        return 0;
    }
    LOGGER_V("registerDocumentStartHook: source length=%lu", (unsigned long) [source length]);

    __block jlong hookId = 0;
    __block bool webViewAvailable = true;
    LOGGER_V("registerDocumentStartHook: dispatching to main thread");
    runOnMainSync(^{
        if (!ctx->webView) {
            LOGGER_E("registerDocumentStartHook: webView is not available in main block");
            webViewAvailable = false;
            return;
        }
        if (!ctx->documentStartHooks) {
            ctx->documentStartHooks = [[NSMutableDictionary alloc] init];
            LOGGER_V("registerDocumentStartHook: allocated new documentStartHooks dictionary");
        }

        hookId = ctx->nextDocumentStartHookId++;
        ctx->documentStartHooks[@(hookId)] = source ?: @"";
        add_document_start_script(ctx->webView, source);
        LOGGER_V("registerDocumentStartHook: registered hook id=%lld", (long long) hookId);
    });

    if (!webViewAvailable) {
        LOGGER_E("registerDocumentStartHook: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return 0;
    }
    return hookId;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    LOGGER_I("unregisterDocumentStartHook: handle=%lld hookId=%lld", (long long) handle, (long long) hookId);
    if (handle == 0) {
        LOGGER_W("unregisterDocumentStartHook: handle is null, throwing NPE");
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("unregisterDocumentStartHook: ctx=%p", (void *) ctx);
    if (!ctx) {
        LOGGER_W("unregisterDocumentStartHook: ctx is null after cast, aborting");
        return;
    }

    __block bool webViewAvailable = true;
    LOGGER_V("unregisterDocumentStartHook: dispatching to main thread");
    runOnMainSync(^{
        if (!ctx->webView) {
            LOGGER_E("unregisterDocumentStartHook: webView is not available in main block");
            webViewAvailable = false;
            return;
        }
        [ctx->documentStartHooks removeObjectForKey:@(hookId)];
        rebuild_document_start_scripts(ctx);
        LOGGER_V("unregisterDocumentStartHook: removed hook id=%lld, rebuilt scripts", (long long) hookId);
    });

    if (!webViewAvailable) {
        LOGGER_E("unregisterDocumentStartHook: webview is not available, throwing exception");
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

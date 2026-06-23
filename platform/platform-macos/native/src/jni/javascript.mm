#include "libs_helpers.h"

static NSString *jstring_to_nsstring(JNIEnv *env, jstring value) {
    if (!value) return nil;
    const char *chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return nil;
    NSString *result = [NSString stringWithUTF8String:chars] ?: @"";
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

static WKUserScript *create_document_start_script(NSString *source) {
    return [[WKUserScript alloc] initWithSource:source ?: @""
                                  injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                               forMainFrameOnly:NO];
}

static void add_document_start_script(WKWebView *webView, NSString *source) {
    WKUserScript *userScript = create_document_start_script(source);
    [webView.configuration.userContentController addUserScript:userScript];
    [userScript release];
}

static void rebuild_document_start_scripts(WebViewContext *ctx) {
    [ctx->webView.configuration.userContentController removeAllUserScripts];
    for (NSNumber *hookId in ctx->documentStartHooks) {
        add_document_start_script(ctx->webView, ctx->documentStartHooks[hookId]);
    }
}

API_EXPORT(jstring, evaluateScript, jlong handle, jstring script) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return nullptr;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return nullptr;

    NSString *source = jstring_to_nsstring(env, script);
    if (env->ExceptionCheck()) return nullptr;

    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block id result = nil;
    __block NSError *error = nil;
    __block bool webViewAvailable = true;

    runOnMainAsync(^{
        if (!ctx->webView) {
            webViewAvailable = false;
            dispatch_semaphore_signal(semaphore);
            return;
        }

        [ctx->webView evaluateJavaScript:source completionHandler:^(id value, NSError *evaluationError) {
            result = [value retain];
            error = [evaluationError retain];
            dispatch_semaphore_signal(semaphore);
        }];
    });

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);

    if (!webViewAvailable) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return nullptr;
    }
    if (error) {
        const char *errorMessage = [[error localizedDescription] UTF8String];
        std::string message = errorMessage ? errorMessage : "WKWebView JavaScript evaluation failed";
        [error release];
        if (result) [result release];
        throw_jni_exception(env, "java/lang/RuntimeException", message.c_str());
        return nullptr;
    }
    if (!result) {
        return nullptr;
    }

    NSString *stringValue = (result == [NSNull null]) ? @"null" : [result description];
    const char *chars = [stringValue UTF8String];
    jstring output = env->NewStringUTF(chars ? chars : "");
    [result release];
    return output;
}

API_EXPORT(jlong, registerDocumentStartHook, jlong handle, jstring script) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return 0;
    }
    if (script == nullptr) {
        throw_jni_exception(env, "java/lang/NullPointerException", "script is null");
        return 0;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return 0;

    NSString *source = jstring_to_nsstring(env, script);
    if (env->ExceptionCheck()) return 0;

    __block jlong hookId = 0;
    __block bool webViewAvailable = true;
    runOnMainSync(^{
        if (!ctx->webView) {
            webViewAvailable = false;
            return;
        }
        if (!ctx->documentStartHooks) {
            ctx->documentStartHooks = [[NSMutableDictionary alloc] init];
        }

        hookId = ctx->nextDocumentStartHookId++;
        ctx->documentStartHooks[@(hookId)] = source ?: @"";
        add_document_start_script(ctx->webView, source);
    });

    if (!webViewAvailable) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
        return 0;
    }
    return hookId;
}

API_EXPORT(void, unregisterDocumentStartHook, jlong handle, jlong hookId) {
    if (handle == 0) {
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    if (!ctx) return;

    __block bool webViewAvailable = true;
    runOnMainSync(^{
        if (!ctx->webView) {
            webViewAvailable = false;
            return;
        }
        [ctx->documentStartHooks removeObjectForKey:@(hookId)];
        rebuild_document_start_scripts(ctx);
    });

    if (!webViewAvailable) {
        throw_jni_exception(env, "java/lang/RuntimeException", "webview is not available");
    }
}

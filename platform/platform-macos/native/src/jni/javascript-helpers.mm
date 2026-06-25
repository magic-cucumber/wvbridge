#import "javascript-helpers.h"

WebViewContext *require_context(JNIEnv *env, jlong handle, const char *operation) {
    if (handle == 0) {
        LOGGER_W("%s: handle is null, throwing NPE", operation);
        throw_jni_exception(env, "java/lang/NullPointerException", "handle is null");
        return nullptr;
    }

    auto *ctx = (WebViewContext *) (uintptr_t) handle;
    LOGGER_V("%s: ctx=%p", operation, (void *) ctx);
    if (!ctx) {
        LOGGER_W("%s: ctx is null after cast, aborting", operation);
    }
    return ctx;
}

NSString *jstring_to_nsstring(JNIEnv *env, jstring value) {
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

WKUserScript *create_document_start_script(NSString *source) {
    LOGGER_V("create_document_start_script: creating script");
    return [[WKUserScript alloc] initWithSource:source ?: @""
                                  injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                               forMainFrameOnly:NO];
}

void add_document_start_script(WKWebView *webView, NSString *source) {
    LOGGER_V("add_document_start_script: adding script to webView=%p", (void *) webView);
    WKUserScript *userScript = create_document_start_script(source);
    [webView.configuration.userContentController addUserScript:userScript];
    [userScript release];
}

void rebuild_document_start_scripts(WebViewContext *ctx) {
    LOGGER_V("rebuild_document_start_scripts: ctx=%p, removing all user scripts", (void *) ctx);
    [ctx->webView.configuration.userContentController removeAllUserScripts];
    for (NSNumber *hookId in ctx->documentStartHooks) {
        add_document_start_script(ctx->webView, ctx->documentStartHooks[hookId]);
    }
}

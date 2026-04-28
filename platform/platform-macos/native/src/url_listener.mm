#import "url_listener.h"

#include "wvbridge/java_caller.h"
#include "wvbridge/java_runtime.h"

#include <string.h>

namespace {

java_caller *create_listener(JNIEnv *env, jobject listener) {
    if (env == nullptr || listener == nullptr) return nullptr;

    java_caller *caller = nullptr;
    java_caller_status status = java_caller_create(env, listener, "accept", "(Ljava/lang/Object;)V", &caller);
    if (status == JAVA_CALLER_ERR_METHOD_NOT_FOUND) {
        status = java_caller_create(env, listener, "invoke", "(Ljava/lang/Object;)Ljava/lang/Object;", &caller);
    }
    return status == JAVA_CALLER_OK ? caller : nullptr;
}

} // namespace

@implementation URLChangeObserver {
    java_caller *_callback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    (void) jvm;
    self = [super init];
    return self;
}

- (void)updateListener:(JNIEnv *)env listener:(jobject)listener {
    java_caller *caller = create_listener(env, listener);
    java_caller *old = nullptr;
    @synchronized(self) {
        old = _callback;
        _callback = caller;
    }
    java_caller_destroy(old);
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) change;
    (void) context;

    if (![keyPath isEqualToString:@"URL"]) return;

    java_caller *caller = nullptr;
    @synchronized(self) {
        caller = java_caller_retain(_callback);
    }
    if (caller == nullptr) return;

    int attached = 0;
    JNIEnv *env = java_runtime_get_env(&attached);
    if (env == nullptr) {
        java_caller_release(caller);
        return;
    }

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    const char *utf8 = (webView.URL.absoluteString ?: @"").UTF8String;
    jstring boxed = env->NewStringUTF(utf8 ? utf8 : "");
    if (boxed != nullptr) {
        jvalue args[1];
        memset(args, 0, sizeof(args));
        args[0].l = boxed;
        java_caller_invoke(caller, args, nullptr);
        env->DeleteLocalRef(boxed);
    } else if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

- (void)dealloc {
    java_caller *old = nullptr;
    @synchronized(self) {
        old = _callback;
        _callback = nullptr;
    }
    java_caller_destroy(old);
    [super dealloc];
}

@end

#import "page_loading.h"

#include "wvbridge/java_caller.h"
#include "wvbridge/java_runtime.h"
#include "wvbridge/utils.h"

#include <string.h>

namespace {

java_caller *create_listener(JNIEnv *env,
                             jobject listener,
                             const char *acceptSignature,
                             const char *invokeSignature) {
    if (env == nullptr || listener == nullptr) return nullptr;

    java_caller *caller = nullptr;
    java_caller_status status = java_caller_create(env, listener, "accept", acceptSignature, &caller);
    if (status == JAVA_CALLER_ERR_METHOD_NOT_FOUND) {
        status = java_caller_create(env, listener, "invoke", invokeSignature, &caller);
    }
    return status == JAVA_CALLER_OK ? caller : nullptr;
}

void set_listener(id owner,
                  java_caller **slot,
                  JNIEnv *env,
                  jobject listener,
                  const char *acceptSignature,
                  const char *invokeSignature) {
    java_caller *caller = create_listener(env, listener, acceptSignature, invokeSignature);

    java_caller *old = nullptr;
    @synchronized(owner) {
        old = *slot;
        *slot = caller;
    }
    java_caller_destroy(old);
}

java_caller *retain_listener(id owner, java_caller *slot) {
    @synchronized(owner) {
        return java_caller_retain(slot);
    }
}

void clear_listener(id owner, java_caller **slot) {
    java_caller *old = nullptr;
    @synchronized(owner) {
        old = *slot;
        *slot = nullptr;
    }
    java_caller_destroy(old);
}

void invoke_string(id owner, java_caller *slot, NSString *value) {
    java_caller *caller = retain_listener(owner, slot);
    if (caller == nullptr) return;

    int attached = 0;
    JNIEnv *env = java_runtime_get_env(&attached);
    if (env == nullptr) {
        java_caller_release(caller);
        return;
    }

    const char *utf8 = (value ?: @"").UTF8String;
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

void invoke_float(id owner, java_caller *slot, float value) {
    java_caller *caller = retain_listener(owner, slot);
    if (caller == nullptr) return;

    jvalue boxed;
    if (java_caller_pack_float(value, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        memset(args, 0, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

void invoke_boolean_string(id owner, java_caller *slot, BOOL value, NSString *message) {
    java_caller *caller = retain_listener(owner, slot);
    if (caller == nullptr) return;

    jvalue boxedBool;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxedBool) != JAVA_CALLER_OK) {
        java_caller_release(caller);
        return;
    }

    int attached = 0;
    JNIEnv *env = java_runtime_get_env(&attached);
    if (env == nullptr) {
        java_caller_delete_global_ref(boxedBool.l);
        java_caller_release(caller);
        return;
    }

    jstring boxedMessage = nullptr;
    if (message != nil) {
        const char *utf8 = message.UTF8String;
        boxedMessage = env->NewStringUTF(utf8 ? utf8 : "");
        if (boxedMessage == nullptr) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            java_caller_delete_global_ref(boxedBool.l);
            java_runtime_detach_env(attached);
            java_caller_release(caller);
            return;
        }
    }

    jvalue args[2];
    memset(args, 0, sizeof(args));
    args[0] = boxedBool;
    args[1].l = boxedMessage;
    java_caller_invoke(caller, args, nullptr);

    if (boxedMessage != nullptr) env->DeleteLocalRef(boxedMessage);
    java_caller_delete_global_ref(boxedBool.l);
    java_runtime_detach_env(attached);
    java_caller_release(caller);
}

} // namespace

@implementation PageLoadingObserver {
    java_caller *_startCallback;
    java_caller *_progressCallback;
    java_caller *_endCallback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    (void) jvm;
    self = [super init];
    return self;
}

- (void)updateStartListener:(JNIEnv *)env listener:(jobject)listener {
    set_listener(self, &_startCallback, env, listener, "(Ljava/lang/Object;)V", "(Ljava/lang/Object;)Ljava/lang/Object;");
}

- (void)updateProgressListener:(JNIEnv *)env listener:(jobject)listener {
    set_listener(self, &_progressCallback, env, listener, "(Ljava/lang/Object;)V", "(Ljava/lang/Object;)Ljava/lang/Object;");
}

- (void)updateEndListener:(JNIEnv *)env listener:(jobject)listener {
    set_listener(self, &_endCallback, env, listener, "(Ljava/lang/Object;Ljava/lang/Object;)V", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) object;
    (void) context;

    if (![keyPath isEqualToString:@"estimatedProgress"]) return;

    const float progress = (float) [[change objectForKey:NSKeyValueChangeNewKey] doubleValue];
    invoke_float(self, _progressCallback, progress);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    (void) navigation;
    invoke_string(self, _startCallback, webView.URL.absoluteString ?: @"");
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    (void) webView;
    (void) navigation;
    invoke_boolean_string(self, _endCallback, YES, nil);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    NSString *reason = [NSString stringWithFormat:@"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
                                                  error.domain ?: @"unknown",
                                                  (long) error.code,
                                                  error.localizedDescription ?: @""];
    invoke_boolean_string(self, _endCallback, NO, reason);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    NSString *reason = [NSString stringWithFormat:@"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
                                                  error.domain ?: @"unknown",
                                                  (long) error.code,
                                                  error.localizedDescription ?: @""];
    invoke_boolean_string(self, _endCallback, NO, reason);
}

- (void)dealloc {
    clear_listener(self, &_startCallback);
    clear_listener(self, &_progressCallback);
    clear_listener(self, &_endCallback);
    [super dealloc];
}

@end

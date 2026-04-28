#import "history_listener.h"

#include "wvbridge/java_caller.h"
#include "wvbridge/utils.h"

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

void set_listener(id owner, java_caller **slot, JNIEnv *env, jobject listener) {
    java_caller *caller = create_listener(env, listener);
    java_caller *old = nullptr;
    @synchronized(owner) {
        old = *slot;
        *slot = caller;
    }
    java_caller_destroy(old);
}

void invoke_boolean(id owner, java_caller *slot, BOOL value) {
    java_caller *caller = nullptr;
    @synchronized(owner) {
        caller = java_caller_retain(slot);
    }
    if (caller == nullptr) return;

    jvalue boxed;
    if (java_caller_pack_boolean(value ? JNI_TRUE : JNI_FALSE, &boxed) == JAVA_CALLER_OK) {
        jvalue args[1];
        memset(args, 0, sizeof(args));
        args[0] = boxed;
        java_caller_invoke(caller, args, nullptr);
        java_caller_delete_global_ref(boxed.l);
    }

    java_caller_release(caller);
}

void clear_listener(id owner, java_caller **slot) {
    java_caller *old = nullptr;
    @synchronized(owner) {
        old = *slot;
        *slot = nullptr;
    }
    java_caller_destroy(old);
}

} // namespace

@implementation HistoryChangeObserver {
    java_caller *_canGoBackCallback;
    java_caller *_canGoForwardCallback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    (void) jvm;
    self = [super init];
    return self;
}

- (void)updateCanGoBackListener:(JNIEnv *)env listener:(jobject)listener {
    set_listener(self, &_canGoBackCallback, env, listener);
}

- (void)updateCanGoForwardListener:(JNIEnv *)env listener:(jobject)listener {
    set_listener(self, &_canGoForwardCallback, env, listener);
}

- (void)emitCurrentState:(WKWebView *)webView {
    if (webView == nil) return;
    invoke_boolean(self, _canGoBackCallback, webView.canGoBack);
    invoke_boolean(self, _canGoForwardCallback, webView.canGoForward);
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) change;
    (void) context;

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    if (webView == nil) return;

    if ([keyPath isEqualToString:@"canGoBack"]) {
        invoke_boolean(self, _canGoBackCallback, webView.canGoBack);
        return;
    }
    if ([keyPath isEqualToString:@"canGoForward"]) {
        invoke_boolean(self, _canGoForwardCallback, webView.canGoForward);
    }
}

- (void)dealloc {
    clear_listener(self, &_canGoBackCallback);
    clear_listener(self, &_canGoForwardCallback);
    [super dealloc];
}

@end

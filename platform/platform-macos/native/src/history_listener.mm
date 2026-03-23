#import "history_listener.h"

#import "java_callback.h"

@implementation HistoryChangeObserver {
    JavaCallbackState _canGoBackCallback;
    JavaCallbackState _canGoForwardCallback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    self = [super init];
    if (self) {
        java_callback_init(&_canGoBackCallback, jvm);
        java_callback_init(&_canGoForwardCallback, jvm);
    }
    return self;
}

- (void)updateCanGoBackListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set(env, &_canGoBackCallback, listener);
}

- (void)updateCanGoForwardListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set(env, &_canGoForwardCallback, listener);
}

- (void)emitCurrentState:(WKWebView *)webView {
    if (webView == nil) return;
    java_callback_call_boolean(&_canGoBackCallback, webView.canGoBack);
    java_callback_call_boolean(&_canGoForwardCallback, webView.canGoForward);
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
        java_callback_call_boolean(&_canGoBackCallback, webView.canGoBack);
        return;
    }
    if ([keyPath isEqualToString:@"canGoForward"]) {
        java_callback_call_boolean(&_canGoForwardCallback, webView.canGoForward);
    }
}

- (void)dealloc {
    java_callback_dispose(&_canGoBackCallback);
    java_callback_dispose(&_canGoForwardCallback);
    [super dealloc];
}

@end

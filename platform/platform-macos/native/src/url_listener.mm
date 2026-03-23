#import "url_listener.h"

#import "java_callback.h"

@implementation URLChangeObserver {
    JavaCallbackState _callback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    self = [super init];
    if (self) {
        java_callback_init(&_callback, jvm);
    }
    return self;
}

- (void)updateListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set(env, &_callback, listener);
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) change;
    (void) context;

    if (![keyPath isEqualToString:@"URL"]) return;

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    java_callback_call_string(&_callback, webView.URL.absoluteString ?: @"");
}

- (void)dealloc {
    java_callback_dispose(&_callback);
    [super dealloc];
}

@end

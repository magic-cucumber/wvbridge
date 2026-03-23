#import "page_loading.h"

#import "java_callback.h"

@implementation PageLoadingObserver {
    JavaCallbackState _startCallback;
    JavaCallbackState _progressCallback;
    JavaCallbackState _endCallback;
}

- (instancetype)initWithJVM:(JavaVM *)jvm {
    self = [super init];
    if (self) {
        java_callback_init(&_startCallback, jvm);
        java_callback_init(&_progressCallback, jvm);
        java_callback_init(&_endCallback, jvm);
    }
    return self;
}

- (void)updateStartListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set(env, &_startCallback, listener);
}

- (void)updateProgressListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set(env, &_progressCallback, listener);
}

- (void)updateEndListener:(JNIEnv *)env listener:(jobject)listener {
    java_callback_set_two_args(env, &_endCallback, listener);
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) object;
    (void) context;

    if (![keyPath isEqualToString:@"estimatedProgress"]) return;

    const float progress = (float) [[change objectForKey:NSKeyValueChangeNewKey] doubleValue];
    java_callback_call_float(&_progressCallback, progress);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    (void) navigation;
    java_callback_call_string(&_startCallback, webView.URL.absoluteString ?: @"");
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    (void) webView;
    (void) navigation;
    java_callback_call_boolean_string(&_endCallback, YES, nil);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    NSString *reason = [NSString stringWithFormat:@"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
                                                  error.domain ?: @"unknown",
                                                  (long) error.code,
                                                  error.localizedDescription ?: @""];
    java_callback_call_boolean_string(&_endCallback, NO, reason);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    NSString *reason = [NSString stringWithFormat:@"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
                                                  error.domain ?: @"unknown",
                                                  (long) error.code,
                                                  error.localizedDescription ?: @""];
    java_callback_call_boolean_string(&_endCallback, NO, reason);
}

- (void)dealloc {
    java_callback_dispose(&_startCallback);
    java_callback_dispose(&_progressCallback);
    java_callback_dispose(&_endCallback);
    [super dealloc];
}

@end

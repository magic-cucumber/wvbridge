#import "webview_events.h"

#include "wvbridge/native_bridge.h"

@interface WebViewEvents ()
- (void)notifyNavigationFailure:(NSError *)error;
@end

@implementation WebViewEvents {
    WKWebView *_webView;
    jlong _pointer;
}

- (instancetype)initWithWebView:(WKWebView *)webView pointer:(jlong)pointer {
    self = [super init];
    if (self == nil) return nil;

    _webView = webView;
    _pointer = pointer;
    _webView.navigationDelegate = self;
    [_webView addObserver:self
               forKeyPath:@"estimatedProgress"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    [_webView addObserver:self
               forKeyPath:@"URL"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    [_webView addObserver:self
               forKeyPath:@"canGoBack"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    [_webView addObserver:self
               forKeyPath:@"canGoForward"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    return self;
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id> *)change
                       context:(void *)context {
    (void) context;

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    if (webView == nil) return;

    if ([keyPath isEqualToString:@"estimatedProgress"]) {
        notify_page_loading_progress_to_jvm(
            _pointer,
            (jfloat) [[change objectForKey:NSKeyValueChangeNewKey] doubleValue]
        );
    } else if ([keyPath isEqualToString:@"URL"]) {
        notify_url_change_to_jvm(
            _pointer,
            (webView.URL.absoluteString ?: @"").UTF8String
        );
    } else if ([keyPath isEqualToString:@"canGoBack"]) {
        notify_can_go_back_change_to_jvm(
            _pointer,
            webView.canGoBack ? JNI_TRUE : JNI_FALSE
        );
    } else if ([keyPath isEqualToString:@"canGoForward"]) {
        notify_can_go_forward_change_to_jvm(
            _pointer,
            webView.canGoForward ? JNI_TRUE : JNI_FALSE
        );
    }
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    (void) navigation;
    notify_page_loading_start_to_jvm(
        _pointer,
        (webView.URL.absoluteString ?: @"").UTF8String
    );
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    (void) webView;
    (void) navigation;
    notify_page_loading_end_to_jvm(_pointer, JNI_TRUE, nullptr);
}

- (void)webView:(WKWebView *)webView
        didFailProvisionalNavigation:(WKNavigation *)navigation
        withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    [self notifyNavigationFailure:error];
}

- (void)webView:(WKWebView *)webView
        didFailNavigation:(WKNavigation *)navigation
        withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    [self notifyNavigationFailure:error];
}

- (void)notifyNavigationFailure:(NSError *)error {
    NSString *reason = [NSString stringWithFormat:
        @"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
        error.domain ?: @"unknown",
        (long) error.code,
        error.localizedDescription ?: @""];
    notify_page_loading_end_to_jvm(_pointer, JNI_FALSE, reason.UTF8String);
}

- (void)dealloc {
    if (_webView != nil) {
        [_webView removeObserver:self forKeyPath:@"estimatedProgress"];
        [_webView removeObserver:self forKeyPath:@"URL"];
        [_webView removeObserver:self forKeyPath:@"canGoBack"];
        [_webView removeObserver:self forKeyPath:@"canGoForward"];
        if (_webView.navigationDelegate == self) {
            _webView.navigationDelegate = nil;
        }
        _webView = nil;
    }
    [super dealloc];
}

@end

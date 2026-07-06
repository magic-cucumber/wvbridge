#import "webview_events.h"

#include "wvbridge/native_bridge.h"
#include <wvbridge/logger.h>

@interface WebViewEvents ()
- (void)notifyNavigationFailure:(NSError *)error;
- (WKNavigationActionPolicy)policyForNavigationAction:(WKNavigationAction *)navigationAction;
@end

@implementation WebViewEvents {
    WKWebView *_webView;
    jlong _pointer;
}

static NSString *NormalizeURLForCompare(NSString *value) {
    if (value == nil) return @"";
    NSString *result = value;
    while (result.length > 1 && [result hasSuffix:@"/"]) {
        result = [result substringToIndex:result.length - 1];
    }
    return result;
}

- (instancetype)initWithWebView:(WKWebView *)webView pointer:(jlong)pointer {
    LOGGER_I("initWithWebView: pointer=%lld", (long long)pointer);
    self = [super init];
    if (self == nil) return nil;

    _webView = webView;
    _pointer = pointer;
    LOGGER_V("initWithWebView: setting navigationDelegate");
    _webView.navigationDelegate = self;
    LOGGER_V("initWithWebView: registering KVO estimatedProgress");
    [_webView addObserver:self
               forKeyPath:@"estimatedProgress"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    LOGGER_V("initWithWebView: registering KVO URL");
    [_webView addObserver:self
               forKeyPath:@"URL"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    LOGGER_V("initWithWebView: registering KVO canGoBack");
    [_webView addObserver:self
               forKeyPath:@"canGoBack"
                  options:NSKeyValueObservingOptionNew
                  context:nil];
    LOGGER_V("initWithWebView: registering KVO canGoForward");
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
    LOGGER_I("observeValueForKeyPath: keyPath=%s", [keyPath UTF8String]);

    WKWebView *webView = [object isKindOfClass:[WKWebView class]] ? (WKWebView *) object : nil;
    if (webView == nil) {
        LOGGER_W("observeValueForKeyPath: webView is nil, aborting");
        return;
    }

    if ([keyPath isEqualToString:@"estimatedProgress"]) {
        LOGGER_V("observeValueForKeyPath: handling estimatedProgress");
        notify_page_loading_progress_to_jvm(
            _pointer,
            (jfloat) [[change objectForKey:NSKeyValueChangeNewKey] doubleValue]
        );
    } else if ([keyPath isEqualToString:@"URL"]) {
        LOGGER_V("observeValueForKeyPath: handling URL");
        notify_url_change_to_jvm(
            _pointer,
            (webView.URL.absoluteString ?: @"").UTF8String
        );
    } else if ([keyPath isEqualToString:@"canGoBack"]) {
        LOGGER_V("observeValueForKeyPath: handling canGoBack");
        notify_can_go_back_change_to_jvm(
            _pointer,
            webView.canGoBack ? JNI_TRUE : JNI_FALSE
        );
    } else if ([keyPath isEqualToString:@"canGoForward"]) {
        LOGGER_V("observeValueForKeyPath: handling canGoForward");
        notify_can_go_forward_change_to_jvm(
            _pointer,
            webView.canGoForward ? JNI_TRUE : JNI_FALSE
        );
    }
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    (void) navigation;
    LOGGER_I("didStartProvisionalNavigation: url=%s", (webView.URL.absoluteString ?: @"").UTF8String);
    notify_page_loading_start_to_jvm(
        _pointer,
        (webView.URL.absoluteString ?: @"").UTF8String
    );
}

- (void)webView:(WKWebView *)webView
    decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction
                    decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    (void) webView;
    LOGGER_I("decidePolicyForNavigationAction: url=%s", (navigationAction.request.URL.absoluteString ?: @"").UTF8String);
    decisionHandler([self policyForNavigationAction:navigationAction]);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    (void) webView;
    (void) navigation;
    LOGGER_I("didFinishNavigation: pointer=%lld", (long long)_pointer);
    notify_page_loading_end_to_jvm(_pointer, JNI_TRUE, nullptr);
}

- (void)webView:(WKWebView *)webView
        didFailProvisionalNavigation:(WKNavigation *)navigation
        withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    LOGGER_I("didFailProvisionalNavigation: domain=%s code=%ld", [error.domain ?: @"unknown" UTF8String], (long)error.code);
    [self notifyNavigationFailure:error];
}

- (void)webView:(WKWebView *)webView
        didFailNavigation:(WKNavigation *)navigation
        withError:(NSError *)error {
    (void) webView;
    (void) navigation;
    LOGGER_I("didFailNavigation: domain=%s code=%ld", [error.domain ?: @"unknown" UTF8String], (long)error.code);
    [self notifyNavigationFailure:error];
}

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView {
    (void) webView;
    LOGGER_I("webViewWebContentProcessDidTerminate: pointer=%lld", (long long)_pointer);
    notify_webview_fatal_error_to_jvm(_pointer, "WK_WEB_CONTENT_PROCESS_DID_TERMINATE");
}

- (void)notifyNavigationFailure:(NSError *)error {
    LOGGER_I("notifyNavigationFailure: domain=%s code=%ld", [error.domain ?: @"unknown" UTF8String], (long)error.code);
    NSString *reason = [NSString stringWithFormat:
        @"wkwebview.navigation.failed: domain=%@, code=%ld, message=%@",
        error.domain ?: @"unknown",
        (long) error.code,
        error.localizedDescription ?: @""];
    LOGGER_V("notifyNavigationFailure: reason=%s", reason.UTF8String);
    notify_page_loading_end_to_jvm(_pointer, JNI_FALSE, reason.UTF8String);
}

- (WKNavigationActionPolicy)policyForNavigationAction:(WKNavigationAction *)navigationAction {
    NSString *url = navigationAction.request.URL.absoluteString ?: @"";
    LOGGER_I("policyForNavigationAction: requesting JVM decision pointer=%lld url=%s", (long long)_pointer, url.UTF8String);
    char* result = notify_navigation_interceptor_to_jvm(_pointer, url.UTF8String);
    if (result == nullptr) {
        LOGGER_W("policyForNavigationAction: no JVM result, allowing navigation");
        return WKNavigationActionPolicyAllow;
    }

    const char action = result[0];
    if (action == '2') {
        LOGGER_I("policyForNavigationAction: decision=cancel url=%s", url.UTF8String);
        free_navigation_interceptor_result(result);
        return WKNavigationActionPolicyCancel;
    }
    if (action == '3') {
        NSString *encoded = [NSString stringWithUTF8String:(result + 1)];
        NSString *redirect = encoded.stringByRemovingPercentEncoding ?: encoded;
        NSString *current = _webView.URL.absoluteString ?: @"";
        LOGGER_I(
            "policyForNavigationAction: decision=redirect url=%s current=%s redirect=%s",
            url.UTF8String,
            current.UTF8String,
            redirect.UTF8String
        );
        if (redirect.length > 0) {
            NSURL *redirectURL = [NSURL URLWithString:redirect];
            if (redirectURL != nil) {
                LOGGER_I("policyForNavigationAction: notifying JVM url change for redirect=%s", redirect.UTF8String);
                notify_url_change_to_jvm(_pointer, redirect.UTF8String);
                if ([NormalizeURLForCompare(redirect) isEqualToString:NormalizeURLForCompare(current)]) {
                    LOGGER_I("policyForNavigationAction: redirect matches current url, reloading");
                    [_webView reload];
                } else {
                    LOGGER_I("policyForNavigationAction: loading redirect url");
                    [_webView loadRequest:[NSURLRequest requestWithURL:redirectURL]];
                }
            } else {
                LOGGER_W("policyForNavigationAction: redirect url is invalid");
            }
        } else {
            LOGGER_W("policyForNavigationAction: redirect url is empty");
        }
        free_navigation_interceptor_result(result);
        return WKNavigationActionPolicyCancel;
    }

    LOGGER_V("policyForNavigationAction: decision=allow url=%s result=%s", url.UTF8String, result);
    free_navigation_interceptor_result(result);
    return WKNavigationActionPolicyAllow;
}

- (void)dealloc {
    LOGGER_I("dealloc: pointer=%lld", (long long)_pointer);
    if (_webView != nil) {
        LOGGER_V("dealloc: removing KVO observers and navigationDelegate");
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

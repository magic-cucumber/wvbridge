#import "ui_delegate.h"
#include <wvbridge/logger.h>

@implementation AllowAllUIDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures {
    LOGGER_I("webView:createWebViewWithConfiguration: url=%s", [navigationAction.request.URL.absoluteString ?: @"" UTF8String]);
    // 如果是用户点击触发的打开新窗口
    if (!navigationAction.targetFrame.isMainFrame) {
        LOGGER_V("webView:createWebViewWithConfiguration: loading request in current webView");
        [webView loadRequest:navigationAction.request]; // 在当前页面强行跳转
    }
    return nil; // 返回 nil 表示不创建新窗口
}

@end
#import "ui_delegate.h"

@implementation AllowAllUIDelegate

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures {
    // 如果是用户点击触发的打开新窗口
    if (!navigationAction.targetFrame.isMainFrame) {
        [webView loadRequest:navigationAction.request]; // 在当前页面强行跳转
    }
    return nil; // 返回 nil 表示不创建新窗口
}

@end
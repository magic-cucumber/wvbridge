#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <jni.h>

@interface WebViewEvents : NSObject<WKNavigationDelegate>

- (instancetype)initWithWebView:(WKWebView *)webView pointer:(jlong)pointer;

@end

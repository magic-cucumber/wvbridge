#pragma once

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

@class WebViewEvents;

struct WebViewContext {
    WKWebView *webView = nil;
    NSWindow *hostWindow = nil;
    NSView *hostView = nil;
    CALayer *rootLayer = nil;
    CALayer *windowLayer = nil;
    WebViewEvents *events = nil;
};
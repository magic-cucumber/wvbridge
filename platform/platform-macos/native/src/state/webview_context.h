#pragma once

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#include <mutex>

#include <wvbridge/javascript.h>

@class WebViewEvents;
@class WVBWebMessageHandler;

struct WebViewContext {
    WKWebView *webView = nil;
    NSWindow *hostWindow = nil;
    NSView *hostView = nil;
    CALayer *rootLayer = nil;
    CALayer *windowLayer = nil;
    WebViewEvents *events = nil;
    WVBWebMessageHandler *webMessageHandler = nil;
    jlong nextDocumentStartHookId = 1;
    NSMutableDictionary<NSNumber *, NSString *> *documentStartHooks = nil;
    jlong nextWebMessageHandlerId = 1;
    std::mutex webMessageHandlersMutex;
    wvbridge::WebMessageHandlers webMessageHandlers;
};

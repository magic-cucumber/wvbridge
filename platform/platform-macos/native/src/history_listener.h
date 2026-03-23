#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <jni.h>

@interface HistoryChangeObserver : NSObject

- (instancetype)initWithJVM:(JavaVM *)jvm;
- (void)updateCanGoBackListener:(JNIEnv *)env listener:(jobject)listener;
- (void)updateCanGoForwardListener:(JNIEnv *)env listener:(jobject)listener;
- (void)emitCurrentState:(WKWebView *)webView;

@end

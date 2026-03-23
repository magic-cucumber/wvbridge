#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <jni.h>

@interface PageLoadingObserver : NSObject<WKNavigationDelegate>

- (instancetype)initWithJVM:(JavaVM *)jvm;
- (void)updateStartListener:(JNIEnv *)env listener:(jobject)listener;
- (void)updateProgressListener:(JNIEnv *)env listener:(jobject)listener;
- (void)updateEndListener:(JNIEnv *)env listener:(jobject)listener;

@end

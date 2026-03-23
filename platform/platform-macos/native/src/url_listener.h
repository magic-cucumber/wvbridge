#pragma once

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#include <jni.h>

@interface URLChangeObserver : NSObject

- (instancetype)initWithJVM:(JavaVM *)jvm;
- (void)updateListener:(JNIEnv *)env listener:(jobject)listener;

@end

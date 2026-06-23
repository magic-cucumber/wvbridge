#pragma once

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <WebKit/WebKit.h>

#import <jni.h>
#import <jawt.h>
#import <jawt_md.h>

#include <algorithm>
#include <cstdint>

#import "utils.h"
#import "ui_delegate.h"
#import "webview_context.h"
#import "webview_events.h"

NSView *find_view_for_layer(CALayer *layer);
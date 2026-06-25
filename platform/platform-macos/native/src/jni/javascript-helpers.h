#pragma once

#import "libs_helpers.h"

#include <wvbridge/logger.h>

WebViewContext *require_context(JNIEnv *env, jlong handle, const char *operation);
NSString *jstring_to_nsstring(JNIEnv *env, jstring value);
WKUserScript *create_document_start_script(NSString *source);
void add_document_start_script(WKWebView *webView, NSString *source);
void rebuild_document_start_scripts(WebViewContext *ctx);

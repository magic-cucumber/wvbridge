#pragma once

#include "libs_helpers.h"

#include <wvbridge/logger.h>

#include <string>

std::string jstring_to_string(JNIEnv *env, jstring value);
WebViewContext *require_context(JNIEnv *env, jlong handle);
WebKitUserScript *add_document_start_script(WebKitWebView *webview, const std::string &source);

#pragma once

#include "libs_helpers.h"

#include <wvbridge/logger.h>

#include <string>

std::wstring jstring_to_wstring(JNIEnv *env, jstring value);
std::string wstring_to_utf8_local(const std::wstring &value);
WebViewContext *require_context(JNIEnv *env, jlong handle);
void throw_hresult(JNIEnv *env, const char *operation, HRESULT hr);
HRESULT ensure_web_message_registered(WebViewContext *ctx);

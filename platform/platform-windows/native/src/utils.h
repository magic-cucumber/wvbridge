#pragma once

#include <jni.h>
#include <string>

#define API_EXPORT(rtn, name, ...) \
    extern "C" JNIEXPORT rtn JNICALL Java_top_kagg886_wvbridge_internal_WebViewBridgePanel_##name(JNIEnv *env, jobject thiz, ##__VA_ARGS__)

void throw_jni_exception(JNIEnv *env, const char *name, const char *message);
std::wstring utf8_to_wstring(const char *utf8);

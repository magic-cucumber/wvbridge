#pragma once

#include <jni.h>

// 与其它平台保持一致的 JNI 导出宏：
// Java_top_kagg886_wvbridge_internal_WebViewBridgePanel_<name>
#define API_EXPORT(rtn, name, ...) \
    extern "C" JNIEXPORT rtn JNICALL Java_top_kagg886_wvbridge_internal_WebViewBridgePanel_##name(JNIEnv *env, jobject thiz, ##__VA_ARGS__)

void throw_jni_exception(JNIEnv *env, const char *name, const char *message);

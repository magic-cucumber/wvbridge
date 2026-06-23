//
// Created by debian on 2026/2/11.
//

#ifndef WVBRIDGE_UTILS_H
#define WVBRIDGE_UTILS_H

#include <jni.h>

#define API_EXPORT(rtn, name, ...) \
    extern "C" JNIEXPORT rtn JNICALL Java_top_kagg886_wvbridge_internal_WebViewBridgePanel_##name(JNIEnv *env, jobject thiz, ##__VA_ARGS__)

void throw_jni_exception(JNIEnv *env, const char *name, const char *message);

#endif //WVBRIDGE_UTILS_H

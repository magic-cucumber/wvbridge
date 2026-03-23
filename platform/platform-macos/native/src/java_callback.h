#pragma once

#import <Foundation/Foundation.h>

#include <jni.h>

struct JavaCallbackState {
    JavaVM *jvm = nullptr;
    jobject listener = nullptr;
    jmethodID method = nullptr;
    BOOL useAccept = YES;
};

void java_callback_init(JavaCallbackState *state, JavaVM *jvm);
void java_callback_set(JNIEnv *env, JavaCallbackState *state, jobject listener);
void java_callback_dispose(JavaCallbackState *state);
void java_callback_call_string(JavaCallbackState *state, NSString *value);
void java_callback_call_float(JavaCallbackState *state, float value);
void java_callback_call_boolean(JavaCallbackState *state, BOOL value);

#pragma once

#include "webview_context.h"

JNIEnv *attach_env(JavaVM *jvm, bool *did_attach);
void detach_env(JavaVM *jvm, bool did_attach);

void replace_listener(JNIEnv *env, JavaListenerState &state, jobject listener);
void replace_listener_with_two_args(JNIEnv *env, JavaListenerState &state, jobject listener);
void clear_listener(JNIEnv *env, JavaListenerState &state);

void notify_string(WebViewContext *ctx, JavaListenerState &state, const wchar_t *value);
void notify_float(WebViewContext *ctx, JavaListenerState &state, float value);
void notify_boolean(WebViewContext *ctx, JavaListenerState &state, bool value);
void notify_boolean_string(WebViewContext *ctx, JavaListenerState &state, bool value, const wchar_t *message);

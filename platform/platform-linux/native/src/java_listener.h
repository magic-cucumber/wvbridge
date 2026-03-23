#pragma once

#include <jni.h>

namespace wvbridge {

struct JavaListener;

JavaListener* java_listener_new(JNIEnv* env);
void java_listener_destroy(JavaListener* listener);
void java_listener_set(JNIEnv* env, JavaListener* listener, jobject callback);
void java_listener_set_two_args(JNIEnv* env, JavaListener* listener, jobject callback);

void java_listener_notify_string(JavaListener* listener, const char* value);
void java_listener_notify_float(JavaListener* listener, float value);
void java_listener_notify_boolean(JavaListener* listener, bool value);
void java_listener_notify_boolean_string(JavaListener* listener, bool value, const char* message);

} // namespace wvbridge

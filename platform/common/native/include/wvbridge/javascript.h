#pragma once

#include <jni.h>

#include <map>
#include <mutex>

namespace wvbridge {

using WebMessageHandlers = std::map<jlong, jobject>;

jlong register_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong& next_handler_id,
    jobject callback
);

void unregister_web_message_handler(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    jlong handler_id
);

void delete_web_message_handler_refs(
    JNIEnv* env,
    std::mutex& mutex,
    WebMessageHandlers& handlers
);

void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const char* message
);

#if defined(_WIN32)
void dispatch_web_message_to_java(
    std::mutex& mutex,
    WebMessageHandlers& handlers,
    const wchar_t* message
);
#endif

} // namespace wvbridge

extern "C" void javascript_on_load(JNIEnv* env);

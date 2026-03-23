#pragma once

#include <jni.h>

#include <atomic>

#include <webkit2/webkit2.h>

namespace wvbridge {

struct HistoryListenerState;

HistoryListenerState* history_listener_state_new(JNIEnv* env);
void history_listener_state_destroy(HistoryListenerState* state);

void history_listener_set_can_go_back_listener(JNIEnv* env, HistoryListenerState* state, jobject listener);
void history_listener_set_can_go_forward_listener(JNIEnv* env, HistoryListenerState* state, jobject listener);

void history_listener_install(WebKitWebView* webview, HistoryListenerState* state, const std::atomic_bool* closing_flag);
void history_listener_uninstall(WebKitWebView* webview, HistoryListenerState* state);
void history_listener_emit_current(WebKitWebView* webview, HistoryListenerState* state);

} // namespace wvbridge

#pragma once

#include <jni.h>

#include <atomic>

#include <webkit2/webkit2.h>

namespace wvbridge {

struct PageLoadingState;

PageLoadingState* page_loading_state_new(JNIEnv* env);
void page_loading_state_destroy(PageLoadingState* state);

void page_loading_set_start_listener(JNIEnv* env, PageLoadingState* state, jobject listener);
void page_loading_set_progress_listener(JNIEnv* env, PageLoadingState* state, jobject listener);
void page_loading_set_end_listener(JNIEnv* env, PageLoadingState* state, jobject listener);

void page_loading_install(WebKitWebView* webview, PageLoadingState* state, const std::atomic_bool* closing_flag);
void page_loading_uninstall(WebKitWebView* webview, PageLoadingState* state);

} // namespace wvbridge

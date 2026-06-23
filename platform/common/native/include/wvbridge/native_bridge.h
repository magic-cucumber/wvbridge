#pragma once

#include <jni.h>

#if defined(_WIN32)
#include <wchar.h>
typedef const wchar_t* wvbridge_native_string;
#else
typedef const char* wvbridge_native_string;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void notify_page_loading_start_to_jvm(jlong pointer, wvbridge_native_string url);
void notify_page_loading_progress_to_jvm(jlong pointer, jfloat progress);
void notify_page_loading_end_to_jvm(jlong pointer, jboolean success, wvbridge_native_string reason);
void notify_url_change_to_jvm(jlong pointer, wvbridge_native_string url);
void notify_can_go_back_change_to_jvm(jlong pointer, jboolean can_go_back);
void notify_can_go_forward_change_to_jvm(jlong pointer, jboolean can_go_forward);
void notify_webview_fatal_error_to_jvm(jlong pointer, wvbridge_native_string cause);

#ifdef __cplusplus
}
#endif

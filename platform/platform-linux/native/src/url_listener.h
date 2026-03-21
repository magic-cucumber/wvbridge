#pragma once

#include <jni.h>

#include <atomic>

#include <webkit2/webkit2.h>

namespace wvbridge {

// URL 变化监听状态（持有 JNI listener 的 GlobalRef + WebKit URL 变化信号连接）。
// 仅在 C++ 层使用；Java/Kotlin 通过 JNI 的 setURLChangeListener() 传入
// Consumer<String> 或 Kotlin Function1<String, Unit>。
struct URLListenerState;

// 创建/销毁
// - env 仅用于获取 JavaVM 指针；不会创建/删除任何 GlobalRef。
URLListenerState* url_listener_state_new(JNIEnv* env);

// 销毁 state，并释放其中持有的 GlobalRef。
// - 可从任意线程调用；内部会按需 AttachCurrentThread 获取 JNIEnv。
void url_listener_state_destroy(URLListenerState* state);

// 设置/替换 URL 变化 listener。
// - listener 允许为 nullptr：表示不再接收 URL 变化通知。
// - 必须从已附加到 JVM 的线程调用（通常是 JNI 入口线程）。
void url_listener_set_listener(JNIEnv* env, URLListenerState* state, jobject listener);

// 安装/卸载 WebKitGTK 的 notify::uri 回调。
// - 仅在 URL 变化时把 url 通知给上层，不参与导航放行决策。
// - closing_flag 用于在 close 流程中避免再进入 JVM。
void url_listener_install(WebKitWebView* webview, URLListenerState* state, const std::atomic_bool* closing_flag);
void url_listener_uninstall(WebKitWebView* webview, URLListenerState* state);

} // namespace wvbridge

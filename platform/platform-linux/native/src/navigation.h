#pragma once

#include <jni.h>

#include <atomic>

#include <webkit/webkit.h>

namespace wvbridge {

// 导航拦截状态（持有 JNI handler 的 GlobalRef + WebKit 信号连接）。
// 仅在 C++ 层使用；Java/Kotlin 通过 JNI 的 setNavigationHandler() 传入 handler。
struct NavigationState;

// 创建/销毁
// - env 仅用于获取 JavaVM 指针；不会创建/删除任何 GlobalRef。
NavigationState* navigation_state_new(JNIEnv* env);

// 销毁 state，并释放其中持有的 GlobalRef。
// - 可从任意线程调用；内部会按需 AttachCurrentThread 获取 JNIEnv。
void navigation_state_destroy(NavigationState* state);

// 设置/替换导航 handler。
// - handler 允许为 nullptr：表示不拦截（默认放行）。
// - 必须从已附加到 JVM 的线程调用（通常是 JNI 入口线程）。
void navigation_set_handler(JNIEnv* env, NavigationState* state, jobject handler);

// 安装/卸载 WebKitGTK 的 decide-policy 回调。
// - closing_flag 用于在 close 流程中避免再进入 JVM（closing==true 时直接拦截/忽略）。
void navigation_install(WebKitWebView* webview, NavigationState* state, const std::atomic_bool* closing_flag);
void navigation_uninstall(WebKitWebView* webview, NavigationState* state);

} // namespace wvbridge

#pragma once

#include <jni.h>

#include <atomic>

#include <webkit/webkit.h>

namespace wvbridge {

// 进度监听状态（持有 JNI listener 的 GlobalRef + WebKit 信号连接）。
// 进度值会被压缩/夹紧到 0.0f..1.0f。
struct ProgressState;

// 创建/销毁
// - env 仅用于获取 JavaVM 指针。
ProgressState* progress_state_new(JNIEnv* env);

// 销毁 state，并释放其中持有的 GlobalRef。
// - 可从任意线程调用；内部会按需 AttachCurrentThread 获取 JNIEnv。
void progress_state_destroy(ProgressState* state);

// 设置/替换进度 listener。
// - listener 允许为 nullptr：表示不监听。
// - 优先解析方法：accept(Object)（java.util.function.Consumer），其次 invoke(Object)（Kotlin Function1）。
void progress_set_listener(JNIEnv* env, ProgressState* state, jobject listener);

// 安装/卸载 WebKitGTK 的 estimated-load-progress 监听。
// - closing_flag 用于在 close 流程中避免再进入 JVM。
void progress_install(WebKitWebView* webview, ProgressState* state, const std::atomic_bool* closing_flag);
void progress_uninstall(WebKitWebView* webview, ProgressState* state);

} // namespace wvbridge

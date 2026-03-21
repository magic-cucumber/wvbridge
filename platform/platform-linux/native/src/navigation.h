#pragma once

#include <atomic>

#include <webkit2/webkit2.h>

namespace wvbridge {

// 导航策略拦截状态（持有 WebKit decide-policy 信号连接）。
struct NavigationState;

NavigationState* navigation_state_new();
void navigation_state_destroy(NavigationState* state);

// 安装/卸载 WebKitGTK 的 decide-policy 回调。
// - 允许所有常规导航。
// - 将新窗口请求（例如 window.open）重定向到当前 WebView。
void navigation_install(WebKitWebView* webview, NavigationState* state, const std::atomic_bool* closing_flag);
void navigation_uninstall(WebKitWebView* webview, NavigationState* state);

} // namespace wvbridge

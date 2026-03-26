package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier

/**
 * Common [WebView] entry points.
 *
 * Platform backends:
 * - Windows: WebView2
 * - Linux: WebKitGTK (`webkit2gtk-4.1`)
 * - macOS: WebKit
 * - Android: Android WebView
 * - iOS: WebKit
 *
 * Desktop platforms are backed by native on-screen rendered views. Because of that, Compose and
 * Swing content cannot be placed above the WebView on Windows, Linux, or macOS. If you need an
 * off-screen rendered browser that can be freely composited with Compose, you need a Chromium-based
 * solution instead. That tradeoff is intentionally out of scope here: this library prefers the
 * simplest possible wrapper around the native WebView shipped by each platform.
 *
 * @param state The remembered state that owns the native WebView instance.
 * @param modifier The modifier applied to the host view.
 */
@Composable
public expect fun WebView(
    state: WebViewState<*> = rememberWebViewState("about:blank"),
    modifier: Modifier = Modifier,
)

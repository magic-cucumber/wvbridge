package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/**
 * Common state holders for [WebView].
 *
 * The public state surface is intentionally small:
 * - [WebViewState.url] mirrors the current top-level document URL and is a good fit for address bars.
 *   It also works with custom URL schemes as long as the underlying native engine accepts them.
 * - [WebViewState.state] exposes the current native readiness and page-loading lifecycle as [LoadingState].
 * - [WebViewState.navigator] exposes imperative navigation operations.
 *
 * [rememberWebViewState] is implemented per platform:
 * - Desktop/JVM: `core/src/jvmMain/kotlin/top/kagg886/wvbridge/state.jvm.kt`
 *   creates `SwingPanelState(WebViewBridgePanel { initialized = true })`.
 *   `initialized` becomes `true` only after
 *   `core/src/jvmMain/kotlin/top/kagg886/wvbridge/internal/WebViewBridgePanel.kt`
 *   finishes `initAndAttach()` inside `addNotify()`, so [LoadingState.NotReady] switches to
 *   [LoadingState.Ready] only after the native desktop view is actually attached.
 *   The native backends behind that panel are WebView2 on Windows, WebKitGTK
 *   (`webkit2gtk-4.1`) on Linux, and WebKit on macOS.
 * - Android: `core/src/androidMain/kotlin/top/kagg886/wvbridge/state.android.kt`
 *   creates an `android.webkit.WebView`, enables the required WebView settings, and then
 *   flips to [LoadingState.Ready] in `LaunchedEffect(Unit)`.
 * - iOS: `core/src/iosMain/kotlin/top/kagg886/wvbridge/state.ios.kt`
 *   creates a `platform.WebKit.WKWebView`, enables JavaScript-related preferences, and then
 *   flips to [LoadingState.Ready] in `LaunchedEffect(Unit)`.
 *
 * Because of that split, non-desktop platforms move from [LoadingState.NotReady] to
 * [LoadingState.Ready] almost immediately during the first composition, while desktop platforms
 * keep [LoadingState.NotReady] until the native peer is fully initialized.
 */
public abstract class WebViewState<T : AutoCloseable> internal constructor(internal val instance: T) :
    AutoCloseable by instance {
    /**
     * The current top-level URL shown by the native WebView.
     *
     * This property is designed to drive address bars and similar UI. It is updated by navigation,
     * redirects, and history traversal. Custom URL schemes are supported when the underlying native
     * engine supports them.
     */
    public var url: String by mutableStateOf("")
        internal set

    /**
     * The current lifecycle state of the native WebView.
     *
     * [LoadingState.NotReady] means the backing view has not finished initialization yet.
     * [LoadingState.Ready] means the native view is ready to receive the initial navigation.
     * After that, the state typically moves between [LoadingState.Loading] and
     * [LoadingState.LoadingEnd].
     */
    public var state: LoadingState by mutableStateOf(LoadingState.NotReady)
        internal set

    /**
     * Imperative navigation controls bound to the same native WebView instance.
     */
    public abstract val navigator: WebViewNavigator
}

/**
 * Creates and remembers a [WebViewState] for the current composition.
 *
 * The returned state starts in [LoadingState.NotReady]. On Android and iOS it becomes
 * [LoadingState.Ready] almost immediately. On desktop it becomes [LoadingState.Ready] only after
 * the Swing/AWT host and its native peer have finished attaching.
 *
 * @param url The initial URL that will be loaded once the native WebView reaches
 *   [LoadingState.Ready].
 * @return A remembered [WebViewState] bound to the current composition.
 */
@Composable
public expect fun rememberWebViewState(url: String = "about:blank"): WebViewState<*>

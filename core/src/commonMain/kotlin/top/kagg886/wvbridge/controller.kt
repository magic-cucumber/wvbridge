package top.kagg886.wvbridge

import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import top.kagg886.wvbridge.bridge.JavaScriptBridge

/**
 * Common controllers for [WebView].
 *
 * The public controller surface is intentionally small:
 * - [WebViewController.url] records the most recent top-level URL the WebView attempted to load and is a
 *   good fit for address bars, including when that navigation fails.
 * - [WebViewController.loadingState] exposes the current native readiness and page-loading lifecycle as [LoadingState].
 * - [WebViewController.navigator] exposes imperative navigation operations.
 *
 * [rememberWebViewController] is implemented per platform:
 * - Desktop/JVM: `core/src/jvmMain/kotlin/top/kagg886/wvbridge/controller.jvm.kt`
 *   creates `SwingPanelController(WebViewBridgePanel { initialized = true })`.
 *   `initialized` becomes `true` only after
 *   `core/src/jvmMain/kotlin/top/kagg886/wvbridge/internal/WebViewBridgePanel.kt`
 *   finishes `initAndAttach()` inside `addNotify()`, so [LoadingState.NotReady] switches to
 *   [LoadingState.Ready] only after the native desktop view is actually attached.
 *   The native backends behind that panel are WebView2 on Windows, WebKitGTK
 *   (`webkit2gtk-4.1`) on Linux, and WebKit on macOS.
 * - Android: `core/src/androidMain/kotlin/top/kagg886/wvbridge/controller.android.kt`
 *   creates an `android.webkit.WebView`, enables the required WebView settings, and then
 *   flips to [LoadingState.Ready] in `LaunchedEffect(Unit)`.
 * - iOS: `core/src/iosMain/kotlin/top/kagg886/wvbridge/controller.ios.kt`
 *   creates a `platform.WebKit.WKWebView`, enables JavaScript-related preferences, and then
 *   flips to [LoadingState.Ready] in `LaunchedEffect(Unit)`.
 *
 * Because of that split, non-desktop platforms move from [LoadingState.NotReady] to
 * [LoadingState.Ready] almost immediately during the first composition, while desktop platforms
 * keep [LoadingState.NotReady] until the native peer is fully initialized.
 */
public abstract class WebViewController<T : AutoCloseable> internal constructor(internal val instance: T) :
    AutoCloseable by instance {
    /**
     * The most recent top-level URL the WebView attempted to load.
     *
     * This property is designed to drive address bars and similar UI. It is updated when a
     * navigation is attempted, including redirects, history traversal, and custom URL schemes. The
     * value remains the attempted URL whether the navigation succeeds or fails, so it may differ
     * from the most recently loaded page.
     */
    public var url: String by mutableStateOf("")
        internal set

    /**
     * The current loading lifecycle of the native WebView.
     *
     * [LoadingState.NotReady] means the backing view has not finished initialization yet.
     * [LoadingState.Ready] means the native view is ready to receive the initial navigation.
     * After that, the loading state typically moves between [LoadingState.Loading] and
     * [LoadingState.LoadingEnd].
     */
    public var loadingState: LoadingState by mutableStateOf(LoadingState.NotReady)
        internal set

    /**
     * Imperative navigation controls bound to the same native WebView instance.
     */
    public abstract val navigator: WebViewNavigator

    /**
     * JavaScript bridge bound to the same native WebView instance.
     *
     * Use this bridge to evaluate scripts in the current page or to register document-start hooks
     * that will run before page scripts on subsequent navigations.
     */
    public abstract val bridge: JavaScriptBridge
}

/**
 * Creates and remembers a [WebViewController] for the current composition.
 *
 * The returned controller starts in [LoadingState.NotReady]. On Android and iOS it becomes
 * [LoadingState.Ready] almost immediately. On desktop it becomes [LoadingState.Ready] only after
 * the Swing/AWT host and its native peer have finished attaching.
 *
 * @param url The initial URL that will be loaded once the native WebView reaches
 *   [LoadingState.Ready].
 * @return A remembered [WebViewController] bound to the current composition.
 */
@Composable
public expect fun rememberWebViewController(url: String = "about:blank"): WebViewController<*>

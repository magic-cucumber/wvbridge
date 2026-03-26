package top.kagg886.wvbridge

/**
 * Shared loading lifecycle definitions for [WebViewState].
 *
 * ```text
 * +-----------+  +-------+  +---------+  +------------+
 * | NotReady  |  | Ready |  | Loading |  | LoadingEnd |
 * +-----+-----+  +---+---+  +----+----+  +------+-----+
 *       |            |           |              |
 *       |----------->|           |              |  native peer becomes available
 *       |            |           |              |  Android/iOS: almost immediate
 *       |            |           |              |  Desktop: after native attach completes
 *       |            |           |              |
 *       |            |---------->|              |  initial load, loadUrl, refresh,
 *       |            |           |              |  goBack, goForward, or redirect
 *       |            |           |              |
 *       |            |           |------------->|  page finished or main-frame error
 *       |            |           |              |
 *       |            |           |              |
 *       |            |           |<-------------|  another navigation starts
 *       |            |           |              |
 * +-----+-----+  +---+---+  +----+----+  +------+-----+
 * | NotReady  |  | Ready |  | Loading |  | LoadingEnd |
 * +-----------+  +-------+  +---------+  +------------+
 * ```
 *
 * State notes:
 * - [NotReady] is the initial state before the native view is ready.
 * - [Ready] is an initialization barrier. It means "safe to issue the first load", not
 *   "the page finished loading".
 * - [Loading] is emitted while the current main-frame navigation is in progress.
 * - [LoadingEnd] represents the terminal result of the current navigation, either success or
 *   failure.
 *
 * After the first load begins, a [WebViewState] usually alternates between [Loading] and
 * [LoadingEnd]. It does not normally return to [Ready] unless a brand new state instance is
 * created.
 */
public sealed interface LoadingState {
    /**
     * The native WebView exists logically, but its platform view is not ready yet.
     *
     * On Android and iOS this state is brief. On desktop it lasts until the native peer is fully
     * attached to the host Swing/AWT component.
     */
    public data object NotReady : LoadingState

    /**
     * The native WebView is ready to accept the initial navigation request.
     *
     * This is a setup milestone rather than a "page is idle" state.
     */
    public object Ready : LoadingState

    /**
     * The current main-frame navigation is running.
     *
     * @property progress A normalized progress value in the `0f..1f` range reported by the native
     *   engine.
     */
    public data class Loading(val progress: Float) : LoadingState

    /**
     * The current main-frame navigation has finished.
     *
     * @property success `true` when the navigation completed successfully, `false` when the native
     *   engine reported an error.
     * @property reason A platform-specific failure reason. This is usually `null` when
     *   [success] is `true`.
     */
    public data class LoadingEnd(val success: Boolean, var reason: String?) : LoadingState
}

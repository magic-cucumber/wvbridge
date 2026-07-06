package top.kagg886.wvbridge

/**
 * Common navigation controls for [WebViewController].
 *
 * This API covers the basic browser operations exposed by all supported backends: moving backward
 * and forward in history, refreshing, stopping the current load, and loading a new URL.
 *
 * Both [Navigator.goBack] and [Navigator.goForward] return a flag describing whether
 * another step in the same direction is still available after the jump that was just requested.
 */
public interface Navigator {
    /**
     * Whether the native history stack currently has a previous entry.
     */
    public val canGoBack: Boolean

    /**
     * Whether the native history stack currently has a next entry.
     */
    public val canGoForward: Boolean

    /**
     * Moves to the previous history entry.
     *
     * @return `true` if another backward jump is still available after this navigation completes,
     *   `false` otherwise.
     */
    public fun goBack(): Boolean

    /**
     * Moves to the next history entry.
     *
     * @return `true` if another forward jump is still available after this navigation completes,
     *   `false` otherwise.
     */
    public fun goForward(): Boolean

    /**
     * Reloads the most recently loaded page.
     *
     * This operation delegates to the native WebView's reload behavior. If the most recent
     * navigation failed, [WebViewController.url] may contain that failed URL while this method reloads
     * the last page that loaded successfully. To retry the most recently attempted URL instead,
     * call [loadUrl] with [WebViewController.url]. After a successful navigation, both approaches reload
     * the same URL.
     */
    public fun refresh()

    /**
     * Stops the current navigation if the platform backend supports cancellation at that moment.
     */
    public fun stop()

    /**
     * Starts a new top-level navigation to [url].
     *
     * @param url The target URL. This may use a custom URL scheme if the underlying native engine
     *   supports it.
     */
    public fun loadUrl(url: String)
}

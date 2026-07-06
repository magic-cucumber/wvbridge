package top.kagg886.wvbridge.interceptor

/**
 * Handles a single top-level navigation URL.
 */
public fun interface InterceptorHandler {
    /**
     * Decides how the WebView should handle [url].
     *
     * Returning [Result.Ignore] means this handler did not make a decision and the dispatcher should
     * continue to the next registered handler. If every handler returns [Result.Ignore], navigation
     * is allowed by default.
     */
    public fun handle(url: String): Result

    /**
     * Navigation decision returned by [InterceptorHandler].
     */
    public sealed interface Result {
        /**
         * Cancel this navigation.
         */
        public data object Rejected : Result

        /**
         * Allow this navigation.
         */
        public data object Allowed : Result

        /**
         * Cancel this navigation and load [url] instead.
         */
        public data class Redirected(val url: String) : Result

        /**
         * Make no decision and continue evaluating later handlers.
         */
        public data object Ignore : Result
    }
}

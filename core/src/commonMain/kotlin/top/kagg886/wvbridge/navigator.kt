package top.kagg886.wvbridge

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/3/23 12:52
 * ================================================
 */

public interface WebViewNavigator {
    public val canGoBack: Boolean
    public val canGoForward: Boolean

    public fun goBack(): Boolean
    public fun goForward(url: String): Boolean
    public fun refresh()
    public fun stop()
    public fun loadUrl(url: String)
}

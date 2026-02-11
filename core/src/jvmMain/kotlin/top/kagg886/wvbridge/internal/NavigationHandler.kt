package top.kagg886.wvbridge

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/2/10 20:53
 * ================================================
 */
internal fun interface NavigationHandler {
    fun handleNavigation(url: String): NavigationResult
    enum class NavigationResult {
        ALLOWED,
        DENIED,
    }
}

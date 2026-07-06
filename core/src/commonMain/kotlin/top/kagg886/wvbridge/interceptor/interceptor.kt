package top.kagg886.wvbridge.interceptor

import top.kagg886.wvbridge.util.CloseHandle

/**
 * Registers callbacks that can decide whether a top-level URL navigation should continue.
 *
 * Navigation interceptors are evaluated in ascending [index] order. Multiple handlers registered
 * with the same [index] keep their platform implementation order. The returned [CloseHandle]
 * removes the handler when closed.
 *
 * This API is intended for URL navigation policy, such as allow lists, external routing, blocking
 * URLs, or redirecting a navigation to another URL. It does not intercept arbitrary subresource
 * requests such as images, scripts, stylesheets, fetch/XHR calls, or response bodies.
 */
public interface Interceptor {
    /**
     * Adds a navigation interceptor.
     *
     * @param index Priority. Lower values run first.
     * @param handler Handler invoked with the requested navigation URL.
     * @return A handle that unregisters [handler].
     */
    public fun registerNavigationInterceptor(index: Int = 0, handler: InterceptorHandler): CloseHandle
}

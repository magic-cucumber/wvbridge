package top.kagg886.wvbridge.interceptor

import top.kagg886.wvbridge.util.CloseHandle

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/7/6 14:02
 * ================================================
 */

public interface Interceptor {
    public fun registerNavigationInterceptor(index: Int = 0, handler: InterceptorHandler): CloseHandle
}

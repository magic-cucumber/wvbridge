package top.kagg886.wvbridge.interceptor

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/7/6 14:07
 * ================================================
 */
public fun interface InterceptorHandler {
    public fun handle(url: String): Result


    public sealed interface Result {
        public data object Rejected : Result
        public data object Allowed : Result
        public data class Redirected(val url: String) : Result
        public data object Ignore : Result
    }
}

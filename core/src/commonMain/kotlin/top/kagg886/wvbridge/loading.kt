package top.kagg886.wvbridge

/**
 * ================================================
 * Author:     886kagg
 * Created on: 2026/3/23 12:48
 * ================================================
 */

public sealed interface LoadingState {
    public data object NotReady : LoadingState
    public object Ready : LoadingState
    public data class Loading(val progress: Float) : LoadingState
    public data class LoadingEnd(val success: Boolean, var reason: String?) : LoadingState
}

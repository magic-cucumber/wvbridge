package top.kagg886.wvbridge.internal.listener

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/6/25 10:06
 * ================================================
 */
public fun interface NativeLoggerReceiver {
    public enum class Level {
        VERBOSE,DEBUG,INFO,WARN,ERROR,ASSERT
    }

    public fun onLoggerReceived(level: Level, tag: String, message: String)
}

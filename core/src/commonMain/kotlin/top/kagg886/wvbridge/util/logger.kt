package top.kagg886.wvbridge.util

/**
 * ================================================
 * Author:     iveou
 * Created on: 2026/6/25 11:03
 * ================================================
 */
public fun interface LoggerReceiver {
    public enum class Level {
        VERBOSE,DEBUG,INFO,WARN,ERROR,ASSERT
    }

    public fun onLoggerReceived(level: Level, tag: String, message: String)

    public companion object {
        private val receivers = mutableSetOf<LoggerReceiver>()


        public fun register(receiver: LoggerReceiver): Unit = check(receivers.add(receiver)) {
            "receiver already registered"
        }

        public fun unregister(receiver: LoggerReceiver): Unit = check(receivers.remove(receiver)) {
            "receiver not unregistered"
        }

        public fun log(level: Level, tag: String, message: String): Unit = receivers.forEach {
            it.onLoggerReceived(level, tag, message)
        }
    }
}

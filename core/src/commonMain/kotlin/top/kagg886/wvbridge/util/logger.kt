package top.kagg886.wvbridge.util

/**
 * Receives log messages emitted by the bridge runtime.
 *
 * Register an implementation with [register] to observe messages from all
 * producers that call [log]. Implementations should avoid throwing exceptions
 * from [onLoggerReceived], because one failing receiver can interrupt
 * notification of later receivers.
 */
public fun interface LoggerReceiver {
    /**
     * Severity of a log message.
     */
    public enum class Level {
        /** Detailed diagnostic output. */
        VERBOSE,

        /** Debug information useful during development. */
        DEBUG,

        /** General informational messages. */
        INFO,

        /** Potentially problematic state that does not stop execution. */
        WARN,

        /** Error state reported by the runtime. */
        ERROR,

        /** Assertion or unrecoverable failure state. */
        ASSERT
    }

    /**
     * Handles a single log message.
     *
     * @param level severity of the message.
     * @param tag component or module that produced the message.
     * @param message formatted message text.
     */
    public fun onLoggerReceived(level: Level, tag: String, message: String)

    /**
     * Process-wide registry used to publish log messages to registered receivers.
     */
    public companion object {
        private val receivers = mutableSetOf<LoggerReceiver>()


        /**
         * Registers [receiver] for subsequent log messages.
         *
         * @throws IllegalStateException if [receiver] has already been registered.
         */
        public fun register(receiver: LoggerReceiver): Unit = check(receivers.add(receiver)) {
            "receiver already registered"
        }

        /**
         * Unregisters [receiver] from subsequent log messages.
         *
         * @throws IllegalStateException if [receiver] is not currently registered.
         */
        public fun unregister(receiver: LoggerReceiver): Unit = check(receivers.remove(receiver)) {
            "receiver not unregistered"
        }

        /**
         * Dispatches a log message to every currently registered receiver.
         *
         * @param level severity of the message.
         * @param tag component or module that produced the message.
         * @param message formatted message text.
         */
        public fun log(level: Level, tag: String, message: String): Unit = receivers.forEach {
            it.onLoggerReceived(level, tag, message)
        }
    }
}

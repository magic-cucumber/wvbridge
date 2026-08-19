package top.kagg886.wvbridge.util

import top.kagg886.wvbridge.util.LoggerReceiver.Companion.log
import top.kagg886.wvbridge.util.LoggerReceiver.Companion.register


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
    public enum class Level(public val code: Int) {
        /** Detailed diagnostic output. */
        VERBOSE(0),

        /** Debug information useful during development. */
        DEBUG(1),

        /** General informational messages. */
        INFO(2),

        /** Potentially problematic state that does not stop execution. */
        WARN(3),

        /** Error state reported by the runtime. */
        ERROR(4),

        /** Assertion or unrecoverable failure state. */
        ASSERT(5);

        public companion object {
            public fun from(code: Int): Level =
                entries.find { it.code == code } ?: throw IllegalArgumentException("no such code: $code")
        }
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
        internal var minLevel = Level.INFO


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
        public fun log(level: Level, tag: String, message: String) {
            if (minLevel > level) return
            receivers.forEach {
                it.onLoggerReceived(level, tag, message)
            }
        }
    }
}

public expect fun LoggerReceiver.Companion.setMinLevel(level: LoggerReceiver.Level)

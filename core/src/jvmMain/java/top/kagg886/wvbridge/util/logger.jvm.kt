package top.kagg886.wvbridge.util

import top.kagg886.wvbridge.internal.listener.NativeBridge

/**
 * Sets the minimum severity for bridge log messages on the JVM.
 *
 * The threshold is applied to Kotlin receivers and forwarded to the native logger.
 *
 * @param level minimum severity to accept.
 */
public actual fun LoggerReceiver.Companion.setMinLevel(level: LoggerReceiver.Level) {
    LoggerReceiver.minLevel = level
    NativeBridge.setMinLevel(level.code)
}

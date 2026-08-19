package top.kagg886.wvbridge.util

public actual fun LoggerReceiver.Companion.setMinLevel(level: LoggerReceiver.Level) {
    LoggerReceiver.minLevel = level
}

package top.kagg886.wvbridge.util

/**
 * A cancellable handle returned by bridge registration APIs.
 *
 * Calling [close] should be idempotent from the caller's perspective: after the first successful
 * close, the underlying registration is no longer active.
 */
public interface CloseHandle : AutoCloseable

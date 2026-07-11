#pragma once

#include <cstddef>

#include <jni.h>

struct WebViewContext;

namespace wvbridge {

struct CloseTicket {
    bool owns_context = false;
    bool shutdown_requested = false;
    std::size_t active_contexts = 0;
    std::size_t closing_contexts = 0;
};

// Registers a fully initialized context. Registration is rejected after JVM
// shutdown has been requested.
bool lifecycle_register(WebViewContext* ctx);

// Atomically removes a context from the active set and marks a close operation
// in flight. A ticket that owns_context=false must never dereference ctx.
CloseTicket lifecycle_begin_close(WebViewContext* ctx, bool jvm_exit_progress);

// Completes a close operation and reports whether the caller must stop GTK.
bool lifecycle_finish_close(const CloseTicket& ticket);

bool lifecycle_shutdown_requested();

// Must run on the GTK thread. It never obtains JNIEnv and never invokes JVM
// callbacks. Returns false only if the context was already structurally empty.
bool destroy_webview_on_gtk_thread(WebViewContext* ctx);

// Must run on the native JNI caller thread while env is valid.
void release_context_jvm_references(JNIEnv* env, WebViewContext* ctx);

} // namespace wvbridge

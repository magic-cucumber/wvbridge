#pragma once

#include "webview_context.h"

void register_history_events(WebViewContext *ctx);
void unregister_history_events(WebViewContext *ctx);
void emit_history_change_events(WebViewContext *ctx);

#pragma once

#include "webview_context.h"

struct WebViewEvents;

WebViewEvents* webview_events_create(WebViewContext* ctx);
void webview_events_destroy(WebViewEvents* events);

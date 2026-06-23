#pragma once

#include <jawt.h>
#include <jawt_md.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <algorithm>
#include <atomic>
#include <functional>
#include <string>

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <gdk/gdkx.h>

#include "utils.h"
#include "gtk.h"
#include "webview_context.h"
#include "webview_events.h"

int clamp_dim(jint v);
::Display *get_xdisplay_from_gdk_default();
::Window get_xid_from_gtk_window(GtkWidget *w);
void x11_ignore_errors(const std::function<void()> &fn);
void x11_set_ewmh_embed_hints(::Display *dpy, ::Window win);
void destroy_ctx_on_gtk_thread(WebViewContext *ctx);
void focus_embedded_webview(WebViewContext *ctx);
gboolean focus_on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
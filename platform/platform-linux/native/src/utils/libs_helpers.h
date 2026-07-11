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
gboolean focus_on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);

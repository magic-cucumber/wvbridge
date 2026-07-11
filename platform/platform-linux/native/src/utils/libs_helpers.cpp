#include "libs_helpers.h"
#include <wvbridge/logger.h>

#include "x11_embed.h"

int clamp_dim(jint v) {
    LOGGER_V("clamp_dim: v=%d", (int)v);
    return (v < 1) ? 1 : (int) v;
}

gboolean focus_on_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    auto *ctx = static_cast<WebViewContext *>(user_data);
    const Time event_time = event ? static_cast<Time>(event->time) : CurrentTime;
    LOGGER_I("focus_on_button_press_cb: widget=%p event=%p ctx=%p event_time=%lu",
             widget, event, ctx, static_cast<unsigned long>(event_time));

    LOGGER_I("focus_embedded_webview: ctx=%p", ctx);
    if (!ctx || ctx->closing.load(std::memory_order_acquire)) {
        LOGGER_W("focus_embedded_webview: null ctx or closing, aborting");
        return FALSE;
    }

    if (ctx->window && GTK_IS_WINDOW(ctx->window)) {
        LOGGER_V("focus_embedded_webview: setting window focus");
        gtk_window_set_accept_focus(GTK_WINDOW(ctx->window), TRUE);
        gtk_window_set_focus(GTK_WINDOW(ctx->window),
                             ctx->webview ? GTK_WIDGET(ctx->webview) : GTK_WIDGET(ctx->window));
    }

    if (ctx->webview) {
        LOGGER_V("focus_embedded_webview: grabbing webview focus");
        gtk_widget_grab_focus(GTK_WIDGET(ctx->webview));
    } else if (ctx->window) {
        LOGGER_V("focus_embedded_webview: grabbing window focus");
        gtk_widget_grab_focus(GTK_WIDGET(ctx->window));
    }

    if (ctx->window) {
        GdkWindow *gdk_window = gtk_widget_get_window(ctx->window);
        if (gdk_window && !gdk_window_is_destroyed(gdk_window)) {
            LOGGER_V("focus_embedded_webview: focusing gdk window=%p event_time=%lu",
                     gdk_window, static_cast<unsigned long>(event_time));
            gdk_window_focus(gdk_window, static_cast<guint32>(event_time));
        } else {
            LOGGER_W("focus_embedded_webview: GdkWindow unavailable or destroyed window=%p ctx=%p",
                     gdk_window, ctx);
        }
    }

    std::string x11_error;
    if (!wvbridge::request_embedded_x11_focus(ctx, event_time, &x11_error)) {
        LOGGER_W("focus_embedded_webview: X11 focus request failed ctx=%p error=%s",
                 ctx, x11_error.c_str());
    } else {
        LOGGER_D("focus_embedded_webview: GTK widget and X11 keyboard focus synchronized ctx=%p child=%lu",
                 ctx, static_cast<unsigned long>(ctx->child_xid));
    }

    // Keep propagating the original button event so WebKit can place the text
    // caret at the clicked position after the native focus transfer.
    return FALSE;
}

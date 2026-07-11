#pragma once

#include <string>

#include <X11/Xlib.h>

struct WebViewContext;

namespace wvbridge {

struct AwtX11Surface {
    Display* display = nullptr; // borrowed and valid only while JAWT is locked
    Window drawable = 0;
    Window root = 0;
    unsigned int width = 1;
    unsigned int height = 1;
    int screen = -1;
    std::string server_name;
};

// Must be called while the JAWT drawing surface is locked.
bool inspect_awt_x11_surface(
    Display* display,
    Window drawable,
    AwtX11Surface* result,
    std::string* error
);

// Must run on the GTK thread while the JAWT surface described by parent is
// still locked. It creates a tracked GDK foreign-parent wrapper, reparents the
// realized GTK window, and synchronously verifies the resulting X11 tree.
bool attach_gtk_window_to_awt(
    WebViewContext* ctx,
    const AwtX11Surface& parent,
    std::string* error
);

// Requests X11 keyboard focus for the embedded child and synchronously checks
// the server response. Must run on the GTK thread. `timestamp` should come
// from the input event when available, or CurrentTime/GDK_CURRENT_TIME.
bool request_embedded_x11_focus(
    WebViewContext* ctx,
    Time timestamp,
    std::string* error
);

} // namespace wvbridge

#include "x11_embed.h"

#include <cctype>
#include <string>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <wvbridge/logger.h>

#include "gtk.h"
#include "webview_context.h"

namespace wvbridge {
namespace {

void set_error(std::string* target, const std::string& value) {
    if (target) *target = value;
}

std::string normalize_server_name(const char* value) {
    std::string name = value ? value : "";
    const std::size_t colon = name.rfind(':');
    const std::size_t dot = name.rfind('.');
    if (colon != std::string::npos && dot != std::string::npos && dot > colon) {
        bool numeric_screen = dot + 1 < name.size();
        for (std::size_t i = dot + 1; i < name.size(); ++i) {
            numeric_screen = numeric_screen && std::isdigit(static_cast<unsigned char>(name[i]));
        }
        if (numeric_screen) name.resize(dot);
    }
    return name;
}

int screen_for_root(Display* display, Window root) {
    if (!display || root == 0) return -1;
    for (int i = 0; i < ScreenCount(display); ++i) {
        if (RootWindow(display, i) == root) return i;
    }
    return -1;
}

bool query_geometry_checked(
    GdkDisplay* gdk_display,
    Display* xdisplay,
    Window window,
    Window* root,
    unsigned int* width,
    unsigned int* height,
    std::string* error
) {
    LOGGER_V("x11.geometry.checked: begin display=%p window=%lu", xdisplay,
             static_cast<unsigned long>(window));
    int x = 0;
    int y = 0;
    unsigned int border = 0;
    unsigned int depth = 0;
    unsigned int local_width = 0;
    unsigned int local_height = 0;
    Window local_root = 0;

    gdk_x11_display_error_trap_push(gdk_display);
    const Status status = XGetGeometry(
        xdisplay, window, &local_root, &x, &y,
        &local_width, &local_height, &border, &depth
    );
    const int x_error = gdk_x11_display_error_trap_pop(gdk_display);
    LOGGER_V("x11.geometry.checked: complete window=%lu status=%d x_error=%d root=%lu size=%ux%u",
             static_cast<unsigned long>(window), status, x_error,
             static_cast<unsigned long>(local_root), local_width, local_height);
    if (x_error != 0 || status == 0) {
        set_error(error, "XGetGeometry failed for window " + std::to_string(window) +
                         ", x_error=" + std::to_string(x_error));
        return false;
    }
    if (root) *root = local_root;
    if (width) *width = local_width;
    if (height) *height = local_height;
    return true;
}

} // namespace

bool inspect_awt_x11_surface(
    Display* display,
    Window drawable,
    AwtX11Surface* result,
    std::string* error
) {
    LOGGER_I("x11.awt.inspect: display=%p drawable=%lu result=%p",
             display, static_cast<unsigned long>(drawable), result);
    if (!display || drawable == 0 || !result) {
        set_error(error, "JAWT returned an incomplete X11 drawing surface");
        LOGGER_E("x11.awt.inspect: invalid input display=%p drawable=%lu result=%p",
                 display, static_cast<unsigned long>(drawable), result);
        return false;
    }

    Window root = 0;
    int x = 0;
    int y = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int border = 0;
    unsigned int depth = 0;
    LOGGER_V("x11.awt.inspect: querying geometry while JAWT lock is held");
    const Status status = XGetGeometry(
        display, drawable, &root, &x, &y,
        &width, &height, &border, &depth
    );
    if (status == 0 || root == 0) {
        set_error(error, "Unable to inspect the locked AWT X11 drawable");
        LOGGER_E("x11.awt.inspect: XGetGeometry failed drawable=%lu status=%d root=%lu",
                 static_cast<unsigned long>(drawable), status, static_cast<unsigned long>(root));
        return false;
    }

    result->display = display;
    result->drawable = drawable;
    result->root = root;
    result->width = width > 0 ? width : 1;
    result->height = height > 0 ? height : 1;
    result->screen = screen_for_root(display, root);
    result->server_name = normalize_server_name(DisplayString(display));
    LOGGER_D("x11.awt.inspect: valid drawable=%lu root=%lu screen=%d size=%ux%u server=%s depth=%u",
             static_cast<unsigned long>(drawable), static_cast<unsigned long>(root), result->screen,
             result->width, result->height, result->server_name.c_str(), depth);
    if (result->screen < 0 || result->server_name.empty()) {
        set_error(error, "Unable to identify the AWT X11 server/screen");
        LOGGER_E("x11.awt.inspect: unidentified server or screen screen=%d server=%s",
                 result->screen, result->server_name.c_str());
        return false;
    }
    return true;
}

bool attach_gtk_window_to_awt(
    WebViewContext* ctx,
    const AwtX11Surface& parent,
    std::string* error
) {
    LOGGER_I("x11.embed: begin ctx=%p parent=%lu expected_root=%lu screen=%d size=%ux%u server=%s",
             ctx, static_cast<unsigned long>(parent.drawable),
             static_cast<unsigned long>(parent.root), parent.screen,
             parent.width, parent.height, parent.server_name.c_str());
    if (!ctx || !ctx->window || !gtk_is_gtk_thread()) {
        set_error(error, "X11 embed must run on the GTK thread with a realized GtkWindow");
        LOGGER_E("x11.embed: invalid state ctx=%p window=%p gtk_thread=%d",
                 ctx, ctx ? ctx->window : nullptr, gtk_is_gtk_thread() ? 1 : 0);
        return false;
    }

    GdkDisplay* gdk_display = gtk_widget_get_display(ctx->window);
    if (!gdk_display || !GDK_IS_X11_DISPLAY(gdk_display)) {
        set_error(error, "GTK is not using the X11 backend");
        LOGGER_E("x11.embed: non-X11 GDK display=%p", gdk_display);
        return false;
    }
    Display* xdisplay = gdk_x11_display_get_xdisplay(gdk_display);
    const std::string gtk_server = normalize_server_name(DisplayString(xdisplay));
    LOGGER_D("x11.embed: phase=validate-server awt_server=%s gtk_server=%s gdk_display=%p xdisplay=%p",
             parent.server_name.c_str(), gtk_server.c_str(), gdk_display, xdisplay);
    if (gtk_server != parent.server_name) {
        set_error(error, "AWT and GTK are connected to different X11 servers");
        LOGGER_E("x11.embed: server mismatch awt=%s gtk=%s",
                 parent.server_name.c_str(), gtk_server.c_str());
        return false;
    }

    Window observed_parent_root = 0;
    unsigned int observed_width = 0;
    unsigned int observed_height = 0;
    if (!query_geometry_checked(
            gdk_display, xdisplay, parent.drawable,
            &observed_parent_root, &observed_width, &observed_height, error)) {
        LOGGER_E("x11.embed: parent is not addressable through GTK X11 connection parent=%lu",
                 static_cast<unsigned long>(parent.drawable));
        return false;
    }
    if (observed_parent_root != parent.root) {
        set_error(error, "AWT drawable root changed while its JAWT surface was locked");
        LOGGER_E("x11.embed: parent root mismatch expected=%lu actual=%lu",
                 static_cast<unsigned long>(parent.root),
                 static_cast<unsigned long>(observed_parent_root));
        return false;
    }

    GdkWindow* child = gtk_widget_get_window(ctx->window);
    if (!child || !GDK_IS_X11_WINDOW(child)) {
        set_error(error, "Realized GtkWindow has no X11 GdkWindow");
        LOGGER_E("x11.embed: invalid child GdkWindow=%p", child);
        return false;
    }
    const Window child_xid = gdk_x11_window_get_xid(child);
    Window child_root = 0;
    if (!query_geometry_checked(gdk_display, xdisplay, child_xid, &child_root, nullptr, nullptr, error)) {
        LOGGER_E("x11.embed: child geometry unavailable child=%lu", static_cast<unsigned long>(child_xid));
        return false;
    }
    if (child_root != observed_parent_root) {
        set_error(error, "GTK child and AWT parent are on different X11 screens");
        LOGGER_E("x11.embed: screen mismatch child_root=%lu parent_root=%lu",
                 static_cast<unsigned long>(child_root),
                 static_cast<unsigned long>(observed_parent_root));
        return false;
    }

    LOGGER_D("x11.embed: phase=create-foreign-parent parent=%lu",
             static_cast<unsigned long>(parent.drawable));
    gdk_x11_display_error_trap_push(gdk_display);
    GdkWindow* foreign_parent = gdk_x11_window_foreign_new_for_display(gdk_display, parent.drawable);
    const int foreign_error = gdk_x11_display_error_trap_pop(gdk_display);
    if (!foreign_parent || foreign_error != 0) {
        if (foreign_parent) g_object_unref(foreign_parent);
        set_error(error, "Unable to create the GDK wrapper for the AWT parent, x_error=" +
                         std::to_string(foreign_error));
        LOGGER_E("x11.embed: foreign parent creation failed wrapper=%p x_error=%d",
                 foreign_parent, foreign_error);
        return false;
    }

    LOGGER_D("x11.embed: phase=reparent child=%lu parent=%lu wrapper=%p size=%ux%u",
             static_cast<unsigned long>(child_xid), static_cast<unsigned long>(parent.drawable),
             foreign_parent, parent.width, parent.height);
    gdk_x11_display_error_trap_push(gdk_display);
    gdk_window_reparent(child, foreign_parent, 0, 0);
    gdk_window_move_resize(child, 0, 0,
                           static_cast<int>(parent.width), static_cast<int>(parent.height));
    gtk_widget_show_all(ctx->window);
    gdk_display_flush(gdk_display);
    const int reparent_error = gdk_x11_display_error_trap_pop(gdk_display);
    if (reparent_error != 0) {
        g_object_unref(foreign_parent);
        set_error(error, "X11 reparent failed, x_error=" + std::to_string(reparent_error));
        LOGGER_E("x11.embed: reparent request failed child=%lu parent=%lu x_error=%d",
                 static_cast<unsigned long>(child_xid),
                 static_cast<unsigned long>(parent.drawable), reparent_error);
        return false;
    }

    Window query_root = 0;
    Window query_parent = 0;
    Window* children = nullptr;
    unsigned int child_count = 0;
    gdk_x11_display_error_trap_push(gdk_display);
    const Status query_status = XQueryTree(
        xdisplay, child_xid, &query_root, &query_parent, &children, &child_count
    );
    const int query_error = gdk_x11_display_error_trap_pop(gdk_display);
    if (children) XFree(children);
    LOGGER_V("x11.embed: verify child=%lu actual_parent=%lu root=%lu status=%d x_error=%d child_count=%u",
             static_cast<unsigned long>(child_xid), static_cast<unsigned long>(query_parent),
             static_cast<unsigned long>(query_root), query_status, query_error, child_count);
    if (query_error != 0 || query_status == 0 || query_parent != parent.drawable) {
        g_object_unref(foreign_parent);
        set_error(error, "X11 tree verification failed after reparent");
        LOGGER_E("x11.embed: verification failed expected_parent=%lu actual_parent=%lu status=%d x_error=%d",
                 static_cast<unsigned long>(parent.drawable),
                 static_cast<unsigned long>(query_parent), query_status, query_error);
        return false;
    }

    ctx->gdk_display = gdk_display;
    ctx->xdisplay = xdisplay;
    ctx->foreign_parent_window = foreign_parent;
    ctx->parent_xid = parent.drawable;
    ctx->child_xid = child_xid;
    ctx->attached.store(true, std::memory_order_release);
    LOGGER_I("x11.embed: complete ctx=%p child=%lu parent=%lu root=%lu",
             ctx, static_cast<unsigned long>(child_xid),
             static_cast<unsigned long>(parent.drawable),
             static_cast<unsigned long>(query_root));
    return true;
}

bool request_embedded_x11_focus(
    WebViewContext* ctx,
    Time timestamp,
    std::string* error
) {
    LOGGER_V("x11.focus: begin ctx=%p timestamp=%lu gtk_thread=%d",
             ctx, static_cast<unsigned long>(timestamp), gtk_is_gtk_thread() ? 1 : 0);
    if (!ctx) {
        set_error(error, "Cannot focus a null WebView context");
        LOGGER_E("x11.focus: null context");
        return false;
    }
    if (!gtk_is_gtk_thread()) {
        set_error(error, "X11 focus request was made outside the GTK thread");
        LOGGER_E("x11.focus: rejected off GTK thread ctx=%p", ctx);
        return false;
    }
    if (ctx->closing.load(std::memory_order_acquire)) {
        set_error(error, "WebView is closing");
        LOGGER_W("x11.focus: context is closing ctx=%p", ctx);
        return false;
    }
    if (!ctx->attached.load(std::memory_order_acquire) ||
        !ctx->gdk_display || !ctx->xdisplay || ctx->child_xid == 0) {
        set_error(error, "Embedded X11 child is not attached");
        LOGGER_W("x11.focus: incomplete attachment ctx=%p attached=%d gdk_display=%p xdisplay=%p child=%lu",
                 ctx, ctx->attached.load(std::memory_order_acquire) ? 1 : 0,
                 ctx->gdk_display, ctx->xdisplay,
                 static_cast<unsigned long>(ctx->child_xid));
        return false;
    }
    if (!GDK_IS_X11_DISPLAY(ctx->gdk_display)) {
        set_error(error, "GTK display is no longer an X11 display");
        LOGGER_E("x11.focus: stored GDK display is not X11 ctx=%p display=%p", ctx, ctx->gdk_display);
        return false;
    }

    LOGGER_D("x11.focus: phase=set-input-focus ctx=%p child=%lu parent=%lu timestamp=%lu",
             ctx, static_cast<unsigned long>(ctx->child_xid),
             static_cast<unsigned long>(ctx->parent_xid),
             static_cast<unsigned long>(timestamp));
    gdk_x11_display_error_trap_push(ctx->gdk_display);
    XSetInputFocus(
        ctx->xdisplay,
        ctx->child_xid,
        RevertToParent,
        timestamp
    );
    const int x_error = gdk_x11_display_error_trap_pop(ctx->gdk_display);
    if (x_error != 0) {
        set_error(error, "XSetInputFocus failed, x_error=" + std::to_string(x_error));
        LOGGER_E("x11.focus: XSetInputFocus failed ctx=%p child=%lu x_error=%d",
                 ctx, static_cast<unsigned long>(ctx->child_xid), x_error);
        return false;
    }

    LOGGER_V("x11.focus: complete ctx=%p child=%lu",
             ctx, static_cast<unsigned long>(ctx->child_xid));
    return true;
}

} // namespace wvbridge

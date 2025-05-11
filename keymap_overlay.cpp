#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <cairo/cairo.h>
#include <iostream>
#include <librsvg/rsvg.h>

int main() {
  // Initialize X11
  Display *display = XOpenDisplay(nullptr);
  if (!display) {
    std::cerr << "Unable to open X display\n";
    return 1;
  }

  Window root = DefaultRootWindow(display);
  XVisualInfo vinfo;
  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);
  XSetWindowAttributes attrs;
  attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
  attrs.border_pixel = 0;
  attrs.background_pixel = 0;
  attrs.override_redirect = True;
  Window win = XCreateWindow(
      display, root, 0, 0, 800, 600, 0, vinfo.depth, InputOutput, vinfo.visual,
      CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attrs);
  XMapWindow(display, win);
  XFlush(display);

  // Create a Cairo surface and context
  cairo_surface_t *surface =
      cairo_xlib_surface_create(display, win, vinfo.visual, 800, 600);
  cairo_t *cr = cairo_create(surface);

  // Set the background to transparent
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);

  // Load and render the SVG
  RsvgHandle *handle = rsvg_handle_new_from_file("keymaps/keymap.svg", nullptr);
  if (handle) {
    rsvg_handle_render_cairo(handle, cr);
    g_object_unref(handle);
  } else {
    std::cerr << "Failed to load SVG file\n";
  }

  // Show the window
  XFlush(display);

  // Event loop
  XEvent event;
  while (true) {
    XNextEvent(display, &event);
    if (event.type == Expose) {
      cairo_set_source_rgba(cr, 0, 0, 0, 0);
      cairo_paint(cr);
      if (handle) {
        rsvg_handle_render_cairo(handle, cr);
      }
      cairo_surface_flush(surface);
    }
  }

  // Clean up
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  XCloseDisplay(display);
  return 0;
}

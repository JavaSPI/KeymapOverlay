#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <ctime>
#include <iostream>
#include <librsvg/rsvg.h>
#include <unistd.h>

#define WINDOW_WIDTH 750
#define WINDOW_HEIGHT 350
#define DISPLAY_TIME 3

Display *display;
int screen;
Window root;
XVisualInfo vinfo;

enum class SvgRegion { TOP, MID_TOP, MID_BOTTOM, BOTTOM, FULL };

Window create_overlay_window(int x, int y) {

  XMatchVisualInfo(display, DefaultScreen(display), 32, TrueColor, &vinfo);
  XSetWindowAttributes attrs;
  attrs.colormap = XCreateColormap(display, root, vinfo.visual, AllocNone);
  attrs.border_pixel = 0;
  attrs.background_pixel = 0;
  attrs.override_redirect = True;
  Window win = XCreateWindow(
      display, root, x, y, WINDOW_WIDTH, WINDOW_HEIGHT, 0, vinfo.depth,
      InputOutput, vinfo.visual,
      CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &attrs);
  XMapWindow(display, win);
  return win;
}

void render_svg(Window win, cairo_surface_t *surface, SvgRegion region) {
  cairo_t *cr = cairo_create(surface);

  // Clear with transparent background
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

  RsvgHandle *handle = rsvg_handle_new_from_file("keymaps/keymap.svg", nullptr);
  if (!handle) {
    std::cerr << "Failed to load keymaps/keymap.svg" << std::endl;
    return;
  }

  RsvgDimensionData dim;
  rsvg_handle_get_dimensions(handle, &dim);

  int crop_y = 0, crop_height = dim.height;
  switch (region) {
  case SvgRegion::TOP:
    crop_height = dim.height / 4;
    break;
  case SvgRegion::MID_TOP:
    crop_y = dim.height / 4;
    crop_height = dim.height / 4;
    break;
  case SvgRegion::MID_BOTTOM:
    crop_y = dim.height / 2;
    crop_height = dim.height / 4;
    break;
  case SvgRegion::BOTTOM:
    crop_y = 3 * dim.height / 4;
    crop_height = dim.height / 4;
    break;
  default:
    break;
  }

  double scale_x = static_cast<double>(WINDOW_WIDTH) / dim.width;
  double scale_y = static_cast<double>(WINDOW_HEIGHT) / crop_height;
  cairo_scale(cr, scale_x, scale_y);
  cairo_translate(cr, 0, -crop_y);

  rsvg_handle_render_cairo(handle, cr);
  g_object_unref(handle);
  cairo_destroy(cr);
}

void show_overlay(SvgRegion region) {
  int screen_width = DisplayWidth(display, screen);
  int screen_height = DisplayHeight(display, screen);
  int x = screen_width - WINDOW_WIDTH - 10;
  int y = screen_height - WINDOW_HEIGHT - 40;

  Window win = create_overlay_window(x, y);
  cairo_surface_t *surface = cairo_xlib_surface_create(
      display, win, vinfo.visual, WINDOW_WIDTH, WINDOW_HEIGHT);

  render_svg(win, surface, region);
  XFlush(display);

  time_t start = time(nullptr);
  while (time(nullptr) - start < DISPLAY_TIME) {
    usleep(10000);
  }

  cairo_surface_destroy(surface);
  XDestroyWindow(display, win);
}

int main() {
  display = XOpenDisplay(nullptr);
  if (!display) {
    std::cerr << "Cannot open display\n";
    return 1;
  }

  screen = DefaultScreen(display);
  root = DefaultRootWindow(display);

  int xi_opcode, event, error;
  if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event,
                       &error)) {
    std::cerr << "X Input extension not available.\n";
    return 1;
  }

  XIEventMask evmask;
  unsigned char mask[(XI_LASTEVENT + 7) / 8] = {0};
  evmask.deviceid = XIAllMasterDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISetMask(mask, XI_RawKeyPress);

  XISelectEvents(display, root, &evmask, 1);
  XFlush(display);

  std::cout << "Listening for keypresses...\n";

  while (true) {
    XEvent ev;
    XNextEvent(display, &ev);

    if (ev.xcookie.type == GenericEvent && ev.xcookie.extension == xi_opcode &&
        XGetEventData(display, &ev.xcookie)) {

      XIRawEvent *re = reinterpret_cast<XIRawEvent *>(ev.xcookie.data);
      int keycode = re->detail;

      KeySym ks = XkbKeycodeToKeysym(display, keycode, 0, 0);
      if (ks == NoSymbol) {
        XFreeEventData(display, &ev.xcookie);
        continue;
      }

      SvgRegion region;
      bool show = true;
      switch (ks) {
      case XK_Help:
        region = SvgRegion::MID_TOP;
        break;
      // case XK_c:
      //   region = SvgRegion::TOP;
      //   break;
      // case XK_v:
      //   region = SvgRegion::MID_TOP;
      //   break;
      // case XK_b:
      //   region = SvgRegion::MID_BOTTOM;
      //   break;
      // case XK_n:
      //   region = SvgRegion::BOTTOM;
      //   break;
      // case XK_A:
      //   region = SvgRegion::FULL;
      //   break;
      default:
        std::cout << "Pressed " << ks << std::endl;
        show = false;
        break;
      }

      if (show) {
        show_overlay(region);
      }

      XFreeEventData(display, &ev.xcookie);
    }
  }

  XCloseDisplay(display);
  return 0;
}

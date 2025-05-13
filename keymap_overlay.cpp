#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo/cairo-xlib.h>
#include <cairo/cairo.h>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <libevdev/libevdev.h>
#include <librsvg/rsvg.h>
#include <unistd.h>

#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#define WINDOW_WIDTH 750
#define WINDOW_HEIGHT 350
#define DISPLAY_TIME 3

Display *display;
int screen;
Window root;
XVisualInfo vinfo;

Window overlay_win = 0;
cairo_surface_t *overlay_surface = nullptr;
time_t overlay_start_time = 0;
bool overlay_visible = false;

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
  // TODO: Think better about how to handle the svgs
  RsvgHandle *handle = rsvg_handle_new_from_file(
      "/home/pemendes/Documents/KeymapOverlay/keymaps/keymap.svg", nullptr);
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

// void show_overlay(SvgRegion region) {
//   int screen_width = DisplayWidth(display, screen);
//   int screen_height = DisplayHeight(display, screen);
//   int x = screen_width - WINDOW_WIDTH - 10;
//   int y = screen_height - WINDOW_HEIGHT - 40;
//
//   Window win = create_overlay_window(x, y);
//   cairo_surface_t *surface = cairo_xlib_surface_create(
//       display, win, vinfo.visual, WINDOW_WIDTH, WINDOW_HEIGHT);
//
//   render_svg(win, surface, region);
//   XFlush(display);
//
//   time_t start = time(nullptr);
//   while (time(nullptr) - start < DISPLAY_TIME) {
//     usleep(10000);
//   }
//
//   cairo_surface_destroy(surface);
//   XDestroyWindow(display, win);
// }

void hide_overlay() {
  if (overlay_visible) {
    // Destroy the Cairo surface first
    if (overlay_surface) {
      cairo_surface_destroy(overlay_surface);
      overlay_surface = nullptr;
    }

    // Unmap and destroy the X11 window
    XUnmapWindow(display, overlay_win);
    XFlush(display);
    XDestroyWindow(display, overlay_win);
    XFlush(display);

    overlay_win = 0;
    overlay_visible = false;
  }
}
void show_overlay(SvgRegion region) {
  if (overlay_visible) {
    cairo_surface_destroy(overlay_surface);
    XDestroyWindow(display, overlay_win);
    overlay_visible = false;
  }

  int screen_width = DisplayWidth(display, screen);
  int screen_height = DisplayHeight(display, screen);
  int x = screen_width - WINDOW_WIDTH - 10;
  int y = screen_height - WINDOW_HEIGHT - 40;

  overlay_win = create_overlay_window(x, y);
  overlay_surface = cairo_xlib_surface_create(
      display, overlay_win, vinfo.visual, WINDOW_WIDTH, WINDOW_HEIGHT);
  render_svg(overlay_win, overlay_surface, region);
  XFlush(display);

  overlay_start_time = time(nullptr);
  overlay_visible = true;
}

std::string find_device_by_id_substring(const std::string &substring) {
  const char *dir_path = "/dev/input/by-id";
  DIR *dir = opendir(dir_path);
  if (!dir) {
    std::cerr << "Cannot open /dev/input/by-id\n";
    return "";
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (strstr(entry->d_name, substring.c_str())) {
      std::string full_link_path = std::string(dir_path) + "/" + entry->d_name;
      char resolved_path[PATH_MAX];
      ssize_t len = readlink(full_link_path.c_str(), resolved_path,
                             sizeof(resolved_path) - 1);
      if (len != -1) {
        resolved_path[len] = '\0';
        std::string event_path =
            "/dev/input/" + std::string(basename(resolved_path));
        closedir(dir);
        return event_path;
      }
    }
  }

  closedir(dir);
  return "";
}

int main() {
  display = XOpenDisplay(nullptr);
  if (!display) {
    std::cerr << "Cannot open display\n";
    return 1;
  }

  screen = DefaultScreen(display);
  root = DefaultRootWindow(display);

  std::string device_path = find_device_by_id_substring("ZMK_Project_Corne_");
  if (device_path.empty()) {
    std::cerr
        << "Could not find input device containing 'ZMK_Project_Corne_'\n";
    return 1;
  }

  int fd = open(device_path.c_str(), O_RDONLY | O_NONBLOCK);
  if (fd < 0) {
    std::cerr << "Failed to open input device: " << device_path << "\n";
    return 1;
  }

  struct libevdev *dev = nullptr;
  if (libevdev_new_from_fd(fd, &dev) < 0) {
    std::cerr << "Failed to init libevdev\n";
    return 1;
  }

  std::cout << "Monitoring " << libevdev_get_name(dev) << "\n";

  struct input_event ev;
  bool ctrl_down = false; // Track CTRL state

  while (true) {
    int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == 0 && ev.type == EV_KEY) {
      if (ev.code == KEY_LEFTCTRL || ev.code == KEY_RIGHTCTRL) {
        ctrl_down = (ev.value != 0); // 1 = press, 0 = release
      }

      if (ev.value == 1) { // Key press
        SvgRegion region;
        bool show = true;

        switch (ev.code) {
        case KEY_HELP:
          region = ctrl_down ? SvgRegion::MID_BOTTOM : SvgRegion::MID_TOP;
          break;
        default:
          // Any other key press hides the overlay
          if (overlay_visible) {
            hide_overlay();
          }
          show = false;
          break;
        }

        if (show) {
          show_overlay(region);
        }
      }
    }

    // Automatically hide after DISPLAY_TIME
    if (overlay_visible &&
        ((time(nullptr) - overlay_start_time) >= DISPLAY_TIME)) {
      hide_overlay();
    }

    usleep(5000);
  }
}

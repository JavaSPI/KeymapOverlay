
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

Display *dpy;
int screen;
Window root;

void launch_xeyes_in_corner() {
    // Get screen size
    int screen_width = DisplayWidth(dpy, screen);
    int screen_height = DisplayHeight(dpy, screen);
    int win_width = 200;
    int win_height = 100;

    // Calculate position for bottom-right corner
    int x = screen_width - win_width - 10;
    int y = screen_height - win_height - 40;

    // Construct command
    char cmd[256];
    snprintf(cmd, sizeof(cmd),
        "xeyes -geometry %dx%d+%d+%d -bw 0 & echo $! > /tmp/xeyes_pid",
        win_width, win_height, x, y);

    system(cmd);
    sleep(3);

    // Kill xeyes
    FILE *f = fopen("/tmp/xeyes_pid", "r");
    if (f) {
        int pid;
        if (fscanf(f, "%d", &pid) == 1) {
            kill(pid, SIGTERM);
        }
        fclose(f);
    }
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    int keycode = XKeysymToKeycode(dpy, XStringToKeysym("v"));
    XGrabKey(dpy, keycode, AnyModifier, root, True,
             GrabModeAsync, GrabModeAsync);
    XSelectInput(dpy, root, KeyPressMask);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);
        if (ev.type == KeyPress) {
            XKeyEvent *kev = &ev.xkey;
            KeySym keysym = XLookupKeysym(kev, 0);
            if (keysym == XK_v) {
                launch_xeyes_in_corner();
            }
        }
    }

    XCloseDisplay(dpy);
    return 0;
}

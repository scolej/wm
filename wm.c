#include <X11/Xlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void fatal(char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
  exit(1);
}

void msg(char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}

Display* dsp;
XColor red;

void manage_new_window(Window win) {
  msg("learned about new window: %d", win);
  XSetWindowBorderWidth(dsp, win, 3);
  XSetWindowBorder(dsp, win, red.pixel);
  XSelectInput(dsp, win, EnterWindowMask);
}

void handle_create_notify(XCreateWindowEvent* event) {
  Window win = event->window;
  msg("create notify for window: %d", win);
}

void handle_map_request(XMapRequestEvent* event) {
  Window win = event->window;
  msg("map request for window: %d", win);
  manage_new_window(win);
  XMapWindow(dsp, win);
}

void handle_enter_notify(XCrossingEvent* event) {
  Window win = event->window;
  long t = event->time;
  msg("focus change on enter-notify to window %d", win);
  XSetInputFocus(dsp, win, RevertToParent, t);
}

int main(int argc, char** argv) {
  dsp = XOpenDisplay(NULL);
  if (!dsp) {
     fatal("could not open display");
  }

  Window root = XDefaultRootWindow(dsp);
  if (!root) {
    fatal("could not open display");
  }

  Colormap cm = XDefaultColormap(dsp, DefaultScreen(dsp));
  if (!cm) {
    fatal("could not get colour-map");
  }

  XColor col;
  Status st;
  col.red = 65535;
  col.green = 0;
  col.blue = 0;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  red = col;

  Window retroot, retparent;
  Window* children;
  unsigned int count;
  st = XQueryTree(dsp, root, &retroot, &retparent, &children, &count);
  if (!st) {
    fatal("couldn't query initial window list");
  }
  for (int i = 0; i < count; i++) {
    manage_new_window(children[i]);
  }

  XSelectInput(dsp, root, SubstructureRedirectMask);

  for (;;) {
    XEvent event;
    XNextEvent(dsp, &event);

    switch (event.type) {
    case CreateNotify:
      handle_create_notify((XCreateWindowEvent*)&event);
      break;
    case MapRequest:
      handle_map_request((XMapRequestEvent*)&event);
      break;
    case EnterNotify:
      handle_enter_notify((XCrossingEvent*)&event);
      break;
    }
  }
}

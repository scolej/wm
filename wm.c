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

void info(char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}

void fine(char* msg, ...) {
  // todo
}

Display* dsp;
XColor red, grey;

void manage_new_window(Window win) {
  info("learned about new window: %d", win);
  XSetWindowBorderWidth(dsp, win, 3);
  XSetWindowBorder(dsp, win, grey.pixel);
  XSelectInput(dsp, win, EnterWindowMask | FocusChangeMask);
  // todo is it necessary to repeat this on every map?
}

void handle_map_request(XMapRequestEvent* event) {
  Window win = event->window;
  info("map request for window: %d", win);
  manage_new_window(win);
  XMapWindow(dsp, win);
}

void handle_enter_notify(XCrossingEvent* event) {
  Window win = event->window;
  long t = event->time;
  info("changing focus on enter-notify to window %d", win);
  XSetInputFocus(dsp, win, RevertToParent, t);
}

struct {
  Window win;
  int start_mouse_x;
  int start_mouse_y;
  int start_win_x;
  int start_win_y;
} drag_state;

void handle_button_press(XButtonEvent* event) {
  Window win = event->subwindow;
  info("started dragging window %d", win);

  int x = event->x_root;
  int y = event->y_root;
  drag_state.win = win;
  drag_state.start_mouse_x = x;
  drag_state.start_mouse_y = y;

  XWindowAttributes attr;
  XGetWindowAttributes(dsp, win, &attr);
  drag_state.start_win_x = attr.x;
  drag_state.start_win_y = attr.y;
}

void handle_button_release(XButtonEvent* event) {
  info("finish drag");
  drag_state.win = 0;
}

void handle_motion(XMotionEvent* event) {
  if (drag_state.win == 0) {
    return;
  }

  int x = event->x_root - drag_state.start_mouse_x + drag_state.start_win_x;
  int y = event->y_root - drag_state.start_mouse_y + drag_state.start_win_y;
  fine("dragging window %d to %d %d", drag_state.win, x, y);
  XMoveWindow(dsp, drag_state.win, x, y);
}

void handle_focus_in(XFocusChangeEvent* event) {
  Window win = event->window;
  XSetWindowBorder(dsp, win, red.pixel);
  info("focus in for window %d", win);
}

void handle_focus_out(XFocusChangeEvent* event) {
  Window win = event->window;
  XSetWindowBorder(dsp, win, grey.pixel);
  info("focus out for window %d", win);
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

  col.red = 1000;
  col.green = 1000;
  col.blue = 1000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  grey = col;

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

  drag_state.win = 0;

  XGrabButton(dsp, 1, Mod4Mask, root, False,
              ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

  XSelectInput(dsp, root, SubstructureRedirectMask);

  for (;;) {
    XEvent event;
    XNextEvent(dsp, &event);

    switch (event.type) {
    case MapRequest:
      handle_map_request((XMapRequestEvent*)&event.xmaprequest);
      break;
    case EnterNotify:
      handle_enter_notify((XCrossingEvent*)&event.xcrossing);
      break;
    case ButtonPress:
      handle_button_press((XButtonEvent*)&event.xbutton);
      break;
    case ButtonRelease:
      handle_button_release((XButtonEvent*)&event.xbutton);
      break;
    case MotionNotify:
      handle_motion((XMotionEvent*)&event.xmotion);
      break;
    case FocusIn:
      handle_focus_in((XFocusChangeEvent*)&event.xfocus);
      break;
    case FocusOut:
      handle_focus_out((XFocusChangeEvent*)&event.xfocus);
      break;
    }
  }
}

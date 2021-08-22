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

// todo pass around a context?
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

enum DragKind {
  MOVE,
  RESIZE_E, RESIZE_W, RESIZE_N, RESIZE_S,
  RESIZE_NW, RESIZE_NE, RESIZE_SW, RESIZE_SE,
};

enum DragHandle {
  LOW, MIDDLE, HIGH
};

struct {
  Window win;
  int start_mouse_x;
  int start_mouse_y;
  int start_win_x;
  int start_win_y;
  int start_win_w;
  int start_win_h;
  enum DragKind kind;
} drag_state;

void handle_button_press(XButtonEvent* event) {
  Window win = event->subwindow;
  int x = event->x_root;
  int y = event->y_root;

  XWindowAttributes attr;
  XGetWindowAttributes(dsp, win, &attr);
  int win_x = attr.x;
  int win_y = attr.y;
  int win_w = attr.width;
  int win_h = attr.height;

  float r = 0.1;
  int x1, x2, y1, y2;
  x1 = win_x + win_w * r;
  x2 = win_x + win_w * (1.0 - r);
  y1 = win_y + win_h * r;
  y2 = win_y + win_h * (1.0 - r);
  info("drag thresholds are %d %d and %d %d", x1, x2, y1, y2);

  enum DragHandle dhx;
  if (x < x1) {
    dhx = LOW;
  } else if (x < x2) {
    dhx = MIDDLE;
  } else {
    dhx = HIGH;
  }

  enum DragHandle dhy;
  if (y < y1) {
    dhy = LOW;
  } else if (y < y2) {
    dhy = MIDDLE;
  } else {
    dhy = HIGH;
  }

  enum DragKind dk;
  if (dhx == LOW && dhy == MIDDLE) {
    dk = RESIZE_W;
  } else if (dhx == HIGH && dhy == MIDDLE) {
    dk = RESIZE_E;
  } else if (dhx == MIDDLE && dhy == LOW) {
    dk = RESIZE_N;
  } else if (dhx == MIDDLE && dhy == HIGH) {
    dk = RESIZE_S;
  } else if (dhx == LOW && dhy == LOW) {
    dk = RESIZE_NW;
  } else if (dhx == HIGH && dhy == LOW) {
    dk = RESIZE_NE;
  } else if (dhx == LOW && dhy == HIGH) {
    dk = RESIZE_SW;
  } else if (dhx == HIGH && dhy == HIGH) {
    dk = RESIZE_SE;
  } else {
    dk = MOVE;
  }

  drag_state.win = win;
  drag_state.start_win_x = win_x;
  drag_state.start_win_y = win_y;
  drag_state.start_win_w = win_w;
  drag_state.start_win_h = win_h;
  drag_state.start_mouse_x = x;
  drag_state.start_mouse_y = y;
  drag_state.kind = dk;

  info("started dragging window %d", win);
}

void handle_button_release(XButtonEvent* event) {
  info("finish drag");
  drag_state.win = 0;
}

void handle_motion(XMotionEvent* event) {
  if (drag_state.win == 0) {
    return;
  }

  int ex = event->x_root;
  int ey = event->y_root;
  int sx = drag_state.start_mouse_x;
  int sy = drag_state.start_mouse_y;
  int wx = drag_state.start_win_x;
  int wy = drag_state.start_win_y;
  int ww = drag_state.start_win_w;
  int wh = drag_state.start_win_h;

  int dx = ex - sx;
  int dy = ey - sy;

  // todo maybe this switch would be better over at drag start
  int xp, xw, yp, yw;
  switch (drag_state.kind) {
  case MOVE:
    xp = 1;
    yp = 1;
    xw = 0;
    yw = 0;
    break;
  case RESIZE_E:
    xp = 0;
    yp = 0;
    xw = 1;
    yw = 0;
    break;
  case RESIZE_W:
    xp = 1;
    yp = 0;
    xw = -1;
    yw = 0;
    break;
  case RESIZE_N:
    xp = 0;
    yp = 1;
    xw = 0;
    yw = -1;
    break;
  case RESIZE_S:
    xp = 0;
    yp = 0;
    xw = 0;
    yw = 1;
    break;
  case RESIZE_NW:
    xp = 1;
    yp = 1;
    xw = -1;
    yw = -1;
    break;
  case RESIZE_NE:
    xp = 0;
    yp = 1;
    xw = 1;
    yw = -1;
    break;
  case RESIZE_SW:
    xp = 1;
    yp = 0;
    xw = -1;
    yw = 1;
    break;
  case RESIZE_SE:
    xp = 0;
    yp = 0;
    xw = 1;
    yw = 1;
    break;
  }

  int x = wx + xp * dx;
  int y = wy + yp * dy;
  int w = ww + xw * dx;
  int h = wh + yw * dy;
  XMoveResizeWindow(dsp, drag_state.win, x, y, w, h);
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

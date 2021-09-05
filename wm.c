#include "buffer.h"
#include "snap.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define BORDER_WIDTH 2
#define BORDER_GAP 3
#define MODMASK Mod1Mask
// todo screen gap

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

// todo
void warn(char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}

// todo
void fine(char* msg, ...) {
  va_list args;
  va_start(args, msg);
  vprintf(msg, args);
  va_end(args);
  printf("\n");
  fflush(stdout);
}

typedef struct {
  int x, y, w, h;
} Rectangle;

#define MAX_NONE 0
#define MAX_BOTH 1
#define MAX_VERT 2
#define MAX_HORI 3

typedef struct {
  Window win;

  Rectangle current_bounds;

  // maximization state
  char max_state;
  // bounds without any maximization applied.
  // only non-zero after a maximization has been applied.
  Rectangle orig_bounds;
} Client;
// todo
// last focus time

Buffer clients;

// Pair of pointer and array index.
// todo smell?
typedef struct {
  void* data;
  unsigned int index;
} PI;

// Finds a client by the window it represents.
PI clients_find(Window w) {
  PI p;
  p.data = NULL;
  p.index = 0;
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    if (c->win == w) {
      fine("found client for window %d at index %d", w, i);
      p.data = c;
      p.index = i;
    }
  }
  return p;
}

// todo pass around a context?
Display *dsp;
XColor red, grey;
unsigned int screen_width, screen_height;
KeyCode key_m;
KeyCode key_v;
KeyCode key_h;

void manage_new_window(Window win) {
  Client c;
  c.win = win;

  Status st;
  XWindowAttributes attr;
  st = XGetWindowAttributes(dsp, win, &attr);
  if (st) {
    c.current_bounds.x = attr.x;
    c.current_bounds.y = attr.y;
    c.current_bounds.w = attr.width;
    c.current_bounds.h = attr.height;
  } else {
    warn("failed to get initial window attributes for %d", win);
    c.current_bounds.x = -1;
    c.current_bounds.y = -1;
    c.current_bounds.w = -1;
    c.current_bounds.h = -1;
  }

  c.max_state = MAX_NONE;

  buffer_add(&clients, &c);

  XSetWindowBorderWidth(dsp, win, BORDER_WIDTH);
  XSetWindowBorder(dsp, win, grey.pixel);
  XSelectInput(dsp, win, EnterWindowMask | FocusChangeMask);

  fine("added new window: %d", win);
}

void handle_map_request(XMapRequestEvent* event) {
  Window win = event->window;
  fine("map request for window: %d", win);
  manage_new_window(win);
  XMapWindow(dsp, win);
}

void handle_enter_notify(XCrossingEvent* event) {
  Window win = event->window;
  long t = event->time;
  fine("changing focus on enter-notify to window %d", win);
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

  if (event->button == 1) {
    // fixme used cached values
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

    XRaiseWindow(dsp, win);
  }
}

void handle_button_release(XButtonEvent* event) {
  if (event->button == 1) {
    if (drag_state.win) {
      info("finish drag");
      drag_state.win = 0;
    }
  } else if (event->button == 3) {
    Window win = event->subwindow;
    info("lowering window: %d", win);
    XLowerWindow(dsp, win);
  }
}

void toggle_maximize(Window win, char kind) {
  fine("toggle maximize for %d", win);

  Client *c = clients_find(win).data;
  if (!c) {
    warn("no client for window: %d", win);
    return;
  }

  Rectangle new;
  Rectangle cur = c->current_bounds;
  if (c->max_state) {
    c->max_state = MAX_NONE;
    new = c->orig_bounds;
  } else {
    c->max_state = kind;
    c->orig_bounds = cur;
    switch (kind) {
    case MAX_BOTH:
      new.x = 0;
      new.y = 0;
      new.w = screen_width - BORDER_WIDTH * 2;
      new.h = screen_height - BORDER_WIDTH * 2;
      break;
    case MAX_VERT:
      new.x = cur.x;
      new.y = 0;
      new.w = cur.w;
      new.h = screen_height - BORDER_WIDTH * 2;
      break;
    case MAX_HORI:
      new.x = 0;
      new.y = cur.y;
      new.w = screen_width - BORDER_WIDTH * 2;
      new.h = cur.h;
      break;
    }
  }
  XMoveResizeWindow(dsp, win, new.x, new.y, new.w, new.h);
}

void handle_key_release(XKeyEvent *event) {
  Window win = event->subwindow;
  KeyCode kc = event->keycode;
  if (kc == key_m) {
    toggle_maximize(win, MAX_BOTH);
  } else if (kc == key_v) {
    toggle_maximize(win, MAX_VERT);
  } else if (kc == key_h) {
    toggle_maximize(win, MAX_HORI);
  } else {
    warn("unhandled key");
  }
}

// Make snap lists for edges: lefts, rights, tops, bottoms. These are the
// values for each edge which we can snap to. The pointer written into
// lefts needs to be freed by the caller. The other pointers share this
// same memory. Returns the number of elements in each list.
unsigned int make_snap_lists(Client* skip, int** ls, int** rs, int** ts, int** bs) {
  unsigned int nc = clients.length - 1;
  // 2 edges per client, 1 edge for the screen
  unsigned int edges = nc * 2 + 1;
  unsigned int cells = edges * 4;
  int* mem = malloc(sizeof(int) * cells);
  *ls = mem;
  *rs = mem + edges;
  *ts = mem + edges * 2;
  *bs = mem + edges * 3;

  unsigned int gap = BORDER_WIDTH * 2 + BORDER_GAP;
  unsigned int count = 0;
  for (unsigned int ci = 0; ci < clients.length; ci++) {
    Client* c = buffer_get(&clients, ci);
    if (skip == c) {
      continue;
    }

    Rectangle rect = c->current_bounds;
    int r = rect.x + rect.w - 1;
    int b = rect.y + rect.h - 1;

    (*ls)[count] = rect.x;
    (*rs)[count] = r;
    (*ts)[count] = rect.y;
    (*bs)[count] = b;
    count++;

    (*ls)[count] = r + gap;
    (*rs)[count] = rect.x - gap;
    (*ts)[count] = b + gap;
    (*bs)[count] = rect.y - gap;
    count++;
  }

  (*ls)[count] = 0;
  (*rs)[count] = screen_width - BORDER_WIDTH * 2 - 1;
  (*ts)[count] = 0;
  (*bs)[count] = screen_height - BORDER_WIDTH * 2 - 1;
  count++;

  assert(count == edges);
  return edges;
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

  // Determine how the mouse delta will impact the size and position of the
  // window.
  //
  // xp, yp   position
  // xw, yw   size
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

  Rectangle rect;
  rect.x = wx + xp * dx;
  rect.y = wy + yp * dy;
  rect.w = ww + xw * dx;
  rect.h = wh + yw * dy;

  Window win = drag_state.win;
  Client *c = clients_find(win).data;
  if (!c) {
    info("no client for: %d", win);
  }

  int *ls, *rs, *ts, *bs;
  unsigned int n = make_snap_lists(c, &ls, &rs, &ts, &bs);
  int l = snap(rect.x, ls, n);
  int r = snap(rect.x + rect.w - 1, rs, n);
  int t = snap(rect.y, ts, n);
  int b = snap(rect.y + rect.h - 1, bs, n);
  free(ls);

  XMoveResizeWindow(dsp, win, l, t, r - l + 1, b - t + 1);
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

void handle_configure(XConfigureEvent* event) {
  Window win = event->window;
  int x = event->x;
  int y = event->y;
  int w = event->width;
  int h = event->height;
  Client* c = clients_find(win).data;
  if (!c) {
    warn("we have no client for window %d", win);
    return;
  }
  c->current_bounds.x = x;
  c->current_bounds.y = y;
  c->current_bounds.w = w;
  c->current_bounds.h = h;
  fine("new position for window %d is [%d %d %d %d]", win, x, y, w, h);
}

void handle_destroy(XDestroyWindowEvent* event) {
  Window win = event->window;
  PI p = clients_find(win);
  if (!p.data) {
    warn("no client for window: %d", win);
    return;
  }
  buffer_remove(&clients, p.index);
  fine("destroyed window %d", win);
}

int error_handler(Display *dsp, XErrorEvent *event) {
  warn("--- X error ---");
  char buffer[128];
  XGetErrorText(dsp, event->error_code, buffer, 128);
  warn("%d %d %s", event->request_code, event->minor_code, buffer);
  return 0;
}

int main(int argc, char** argv) {
  dsp = XOpenDisplay(NULL);
  if (!dsp) {
     fatal("could not open display");
  }

  XSetErrorHandler(error_handler);

  Window root = XDefaultRootWindow(dsp);
  if (!root) {
    fatal("could not open display");
  }

  int default_screen = DefaultScreen(dsp);

  Colormap cm = XDefaultColormap(dsp, default_screen);
  if (!cm) {
    fatal("could not get colour-map");
  }

  screen_width = DisplayWidth(dsp, default_screen);
  screen_height = DisplayHeight(dsp, default_screen);

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

  col.red = 30000;
  col.green = 30000;
  col.blue = 30000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  grey = col;

  buffer_init(&clients, 500, sizeof(Client));

  Window retroot, retparent;
  Window* children;
  unsigned int count;
  st = XQueryTree(dsp, root, &retroot, &retparent, &children, &count);
  if (!st) {
    fatal("couldn't query initial window list");
  }
  for (unsigned int i = 0; i < count; i++) {
    manage_new_window(children[i]);
  }
  XFree(children);

  drag_state.win = 0;

  XGrabButton(dsp, AnyButton, MODMASK, root, False,
              ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

  key_m = XKeysymToKeycode(dsp, XK_M);
  key_v = XKeysymToKeycode(dsp, XK_V);
  key_h = XKeysymToKeycode(dsp, XK_H);
  XGrabKey(dsp, key_m, MODMASK, root, False, GrabModeAsync, GrabModeAsync);
  XGrabKey(dsp, key_v, MODMASK, root, False, GrabModeAsync, GrabModeAsync);
  XGrabKey(dsp, key_h, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  XSelectInput(dsp, root, SubstructureRedirectMask | SubstructureNotifyMask);

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
    case KeyPress:
      handle_key_release((XKeyEvent*)&event.xkey);
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
    case ConfigureNotify:
      handle_configure((XConfigureEvent*)&event.xconfigure);
      break;
    case DestroyNotify:
      handle_destroy((XDestroyWindowEvent*)&event.xdestroywindow);
      break;
    }
  }
}

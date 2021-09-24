#include "buffer.h"
#include "clients.h"
#include "snap.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// timeout when switching windows for selected client to go to top of stack
#define SWITCH_TIMEOUT_MS 1000

#define SNAP_DIST 30

// what fraction of window width/height do edge handles occupy?
#define HANDLE_FRAC 0.2

#define BORDER_WIDTH 4
#define BORDER_GAP 2
// todo screen gap

/* #define MODMASK Mod4Mask */
/* #define MODL XK_Super_L */
/* #define MODR XK_Super_R */

#define MODMASK Mod1Mask
#define MODL XK_Alt_L
#define MODR XK_Alt_R

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
  printf("WARN - ");
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

#define MAX_NONE 0
#define MAX_BOTH 1
#define MAX_VERT 2
#define MAX_HORI 3

// todo pass around a context?
Display *dsp;
Window root;
XColor focused_colour, unfocused_colour, switching_colour;
unsigned int screen_width, screen_height;

Window last_focused_window = 0;

// we'll set this to 1 when we start cycling through windows.
// when timer expires, we'll set it back to 0 and update the window's focus time.
char transient_switching = 0;

// index into window_focus_history
unsigned int transient_switching_index = 0;

// flag indicating timer has expired
char timer_expired = 0;

// char skip_enter = 0;

void manage_new_window(Window win) {
  if (clients_find(win).data) {
    warn("already tracking %x", win);
    return;
  }

  Client c;
  c.win = win;

  Status st;
  XWindowAttributes attr;
  st = XGetWindowAttributes(dsp, win, &attr);
  if (!st) {
    warn("failed to get initial window attributes for %x", win);
    return;
  }

  if (attr.override_redirect) {
    info("ignoring override_redirect window");
    return;
  }

  c.current_bounds.x = attr.x;
  c.current_bounds.y = attr.y;
  c.current_bounds.w = attr.width;
  c.current_bounds.h = attr.height;

  c.max_state = MAX_NONE;
  c.border_width = BORDER_WIDTH;

  clients_add(&c);

  XSetWindowBorderWidth(dsp, win, c.border_width);
  XSetWindowBorder(dsp, win, unfocused_colour.pixel);
  XSelectInput(dsp, win, EnterWindowMask | FocusChangeMask);

  info("added new window: %x", win);
}

void remove_window(Window win) {
  clients_del(win);
  info("destroyed window %x", win);
}

#define MIN(a, b) ( a < b ? a : b )
#define MAX(a, b) ( a > b ? a : b )

void fetch_apply_normal_hints(Window win) {
  XSizeHints *sh = XAllocSizeHints();
  assert(sh);
  long supplied;
  Status st = XGetWMNormalHints(dsp, win, sh, &supplied);
  if (!st) {
    goto clean;
  }

  Client *c = clients_find(win).data;
  if (!c) {
    warn("no client for window while applying size hints: %x", win);
    goto clean;
  }

  int w = c->current_bounds.w;
  int h = c->current_bounds.h;

  if (sh->flags & PMinSize) {
    info("using minimum size");
    w = MAX(sh->min_width, w);
    h = MAX(sh->min_height, h);
  }

  if (sh->flags & PBaseSize) {
    info("using base size");
    w = MAX(sh->base_width, w);
    h = MAX(sh->base_height, h);
  }

  XResizeWindow(dsp, win, w, h);

 clean:
  XFree(sh);
}

void handle_map_request(XMapRequestEvent* event) {
  Window win = event->window;
  fine("map request for window: %x", win);
  manage_new_window(event->window);
  fetch_apply_normal_hints(win);
  XMapWindow(dsp, win);
}

void handle_map_notify(XMapEvent* event) {
  // todo
}

void handle_unmap_notify(XUnmapEvent* event) {
  remove_window(event->window);
}

void handle_enter_notify(XCrossingEvent* event) {
  // if (skip_enter) {
  //   skip_enter = 0;
  //   return;
  // }

  Window win = event->window;
  long t = event->time;
  info("changing focus on enter-notify to window %x", win);
  XSetInputFocus(dsp, win, RevertToParent, t);
}

// 9 different ways to move/resize, depending on
// where the initial press happens
enum DragKind {
  MOVE,
  RESIZE_E, RESIZE_W, RESIZE_N, RESIZE_S,
  RESIZE_NW, RESIZE_NE, RESIZE_SW, RESIZE_SE,
};

// intersection of 3 regions on two axes creates
// the different drag handles
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

// flag indicating whether a modifier press is followed by something else.
// if it's not, then we can use it to switch windows.
char prime_mod = 0;

void handle_button_press(XButtonEvent* event) {
  prime_mod = 0;

  Window win = event->subwindow;
  int x = event->x_root;
  int y = event->y_root;

  if (event->button == 1) {
    Client *c = clients_find(win).data;
    if (!c) {
      warn("no client for window: %x", win);
      return;
    }

    Rectangle bounds = c->current_bounds;

    int x1, x2, y1, y2;
    x1 = bounds.x + bounds.w * HANDLE_FRAC;
    x2 = bounds.x + bounds.w * (1.0 - HANDLE_FRAC);
    y1 = bounds.y + bounds.h * HANDLE_FRAC;
    y2 = bounds.y + bounds.h * (1.0 - HANDLE_FRAC);

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
    drag_state.start_win_x = bounds.x;
    drag_state.start_win_y = bounds.y;
    drag_state.start_win_w = bounds.w;
    drag_state.start_win_h = bounds.h;
    drag_state.start_mouse_x = x;
    drag_state.start_mouse_y = y;
    drag_state.kind = dk;

    // any manual resize/move reverts the maximization state
    // todo this should really happen on move, not press
    c->max_state = MAX_NONE;

    XRaiseWindow(dsp, win);
  }
}

void handle_button_release(XButtonEvent* event) {
  if (event->button == 1) {
    if (drag_state.win) {
      drag_state.win = 0;
    }
  } else if (event->button == 3) {
    Window win = event->subwindow;
    XLowerWindow(dsp, win);
  }
}

void toggle_maximize(Window win, char kind) {
  fine("toggle maximize for %x", win);

  Client *c = clients_find(win).data;
  if (!c) {
    warn("no client for window: %x", win);
    return;
  }

  Rectangle new;
  Rectangle cur = c->current_bounds;
  if (c->max_state == kind) {
    // restore orig if we're toggling the same kind as current
    c->max_state = MAX_NONE;
    new = c->orig_bounds;
  } else {
    if (c->max_state == MAX_NONE) {
      // only stash current bounds from no-maximization-state
      c->orig_bounds = cur;
    }
    Rectangle orig = c->orig_bounds;
    c->max_state = kind;
    switch (kind) {
    case MAX_BOTH:
      new.x = 0;
      new.y = 0;
      new.w = screen_width - BORDER_WIDTH * 2;
      new.h = screen_height - BORDER_WIDTH * 2;
      break;
    case MAX_VERT:
      new.x = orig.x;
      new.y = 0;
      new.w = orig.w;
      new.h = screen_height - BORDER_WIDTH * 2;
      break;
    case MAX_HORI:
      new.x = 0;
      new.y = orig.y;
      new.w = screen_width - BORDER_WIDTH * 2;
      new.h = orig.h;
      break;
    }
  }
  XMoveResizeWindow(dsp, win, new.x, new.y, new.w, new.h);
}

void track_focus_change(Client *focused) {
  Window win = focused->win;
  XSetWindowBorder(dsp, win, focused_colour.pixel);
  clients_focus_raise(win);
}

void timer_handler(int signal) {
  timer_expired = 1;
}

void timer_has_expired() {
  fine("timer expired");

  transient_switching = 0;

  Client *c = clients_find(last_focused_window).data;
  if (!c) {
    info("timer expired with focus on un-tracked window: %x", last_focused_window);
    return;
  }

  track_focus_change(c);
}

void switch_next_window() {
  if (++transient_switching_index >= window_focus_history.length) {
    transient_switching_index = 0;
  }

  Window win = window_history_get(transient_switching_index);

  fine("setting focus to %x", win);
  XRaiseWindow(dsp, win);
  XSetInputFocus(dsp, win, RevertToParent, CurrentTime);
}

void switch_windows() {
  if (!transient_switching) {
    transient_switching = 1;
    transient_switching_index = 0;
  }

  // reset the timer
  struct itimerval t;
  t.it_value.tv_sec = 1;
  t.it_value.tv_usec = 0;
  t.it_interval.tv_sec = 0;
  t.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &t, NULL);

  switch_next_window();
}

void maximize() {
  Window win = window_history_get(0);
  toggle_maximize(win, MAX_BOTH);
}

void maximize_horiz() {
  Window win = window_history_get(0);
  toggle_maximize(win, MAX_HORI);
}

void maximize_vert() {
  Window win = window_history_get(0);
  toggle_maximize(win, MAX_VERT);
}

void close_window() {
  Window win = window_history_get(0);
  XDestroyWindow(dsp, win);
}

void lower() {
  Window win0 = window_history_get(0);
  Window win1 = window_history_get(1);

  Client *c = clients_find(win1).data;
  if (!c) {
    fine("lower - no client for window %x", win1);
    return;
  }

  clients_focus_lower(win0);
  XLowerWindow(dsp, win0);
  XWarpPointer(dsp, 0, win1,
               0, 0, 0, 0,
               c->current_bounds.w / 2,
               c->current_bounds.h / 2);
  XSetInputFocus(dsp, win1, RevertToParent, CurrentTime);

  info("%x goes to back, focus %x", win0, win1);
}

void log_debug() {
  info("--- debug ---");
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    info("%15x", c->win);
  }
  info("------");
}

void toggle_border() {
  Window win = window_history_get(0);
  Client *c = clients_find(win).data;
  if (!c) {
    fine("toggle_borderd - no client for window %x", win);
    return;
  }

  if (c->border_width) {
    c->border_width = 0;
  } else {
    c->border_width = BORDER_WIDTH;
  }
  XSetWindowBorderWidth(dsp, win, c->border_width);
}

typedef struct {
  KeySym sym;
  unsigned int mods;

  // filled during init
  KeyCode kc;

  void (*binding)();
} Key;

Key keys[] = {
  { XK_M, MODMASK, 0, maximize },
  { XK_V, MODMASK, 0, maximize_vert},
  { XK_H, MODMASK, 0, maximize_horiz},
  { XK_D, MODMASK, 0, log_debug},
  { XK_Q, MODMASK, 0, close_window},
  { XK_B, MODMASK, 0, toggle_border},
  { XK_Escape, MODMASK, 0, lower },
  { MODL, 0, 0, switch_windows },
  { MODR, 0, 0, switch_windows },
};

KeyCode kc_modl, kc_modr;

void handle_key_press(XKeyEvent *event) {
  KeyCode kc = event->keycode;
  if (kc == kc_modl || kc == kc_modr) {
    prime_mod = 1;
  } else {
    prime_mod = 0;
  }
}

void handle_key_release(XKeyEvent *event) {
  KeyCode ekc = event->keycode;

  // special handling to avoid triggering mod binding if something else was pressed
  if ((ekc == kc_modl || ekc == kc_modr) && prime_mod == 0) {
    return;
  }

  for (unsigned int i = 0; i < sizeof(keys) / sizeof(Key); i++) {
    Key k = keys[i];
    if (k.kc == ekc) {
      info("running binding for key index %d", i);
      (k.binding)();
    }
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
    info("no client for dragging: %x", win);
    return;
  }

  int *ls, *rs, *ts, *bs;
  unsigned int n = make_snap_lists(c, &ls, &rs, &ts, &bs);
  int l = snap(rect.x, ls, n, SNAP_DIST);
  int r = snap(rect.x + rect.w - 1, rs, n, SNAP_DIST);
  int t = snap(rect.y, ts, n, SNAP_DIST);
  int b = snap(rect.y + rect.h - 1, bs, n, SNAP_DIST);
  free(ls);

  XMoveResizeWindow(dsp, win, l, t, r - l + 1, b - t + 1);
}

void handle_focus_in(XFocusChangeEvent *event) {
  Window win = event->window;

  fine("--- focus-in %x ---", win);

  if (event->mode == NotifyGrab || event->mode == NotifyUngrab) {
    fine("discard grab focus-in");
    return;
  }

  last_focused_window = win;
  fine("focus in for window %x", win);

  if (transient_switching) {
    // focus change is temporary
    XSetWindowBorder(dsp, win, switching_colour.pixel);
    return;
  }

  Client *c = clients_find(win).data;
  if (!c) {
    fine("focus-in for un-tracked window: %x", win);
    return;
  }

  track_focus_change(c);
}

void handle_focus_out(XFocusChangeEvent* event) {
  fine("--- focus-out ---");

  if (event->mode == NotifyGrab || event->mode == NotifyUngrab) {
    fine("discard grab focus-out");
    return;
  }

  Window win = event->window;
  XSetWindowBorder(dsp, win, unfocused_colour.pixel);
  fine("focus out for window %x", win);
}

void handle_configure(XConfigureEvent* event) {
  Window win = event->window;

  fine("--- configure %x ---", win);

  int x = event->x;
  int y = event->y;
  int w = event->width;
  int h = event->height;

  Client* c = clients_find(win).data;
  if (!c) {
    fine("no client for window %x", win);
    return;
  }

  c->current_bounds.x = x;
  c->current_bounds.y = y;
  c->current_bounds.w = w;
  c->current_bounds.h = h;

  fine("new position for window %x is [%d %d %d %d]", win, x, y, w, h);
}

void handle_destroy(XDestroyWindowEvent* event) {
  remove_window(event->window);
}

int error_handler(Display *dsp, XErrorEvent *event) {
  char buffer[128];
  XGetErrorText(dsp, event->error_code, buffer, 128);
  warn("XError %d %d %s", event->request_code, event->minor_code, buffer);
  return 0;
}

void handle_reparent(XReparentEvent *event) {
  Window win = event->window;
  if (event->parent != root) {
    info("removing client after reparent for window: %x", win);
    remove_window(win);
  }
}

// grab and dispatch all events in the queue
void handle_xevents() {
  while (XPending(dsp)) {
    XEvent event;
    XNextEvent(dsp, &event);

    switch (event.type) {
    case MapRequest:
      handle_map_request((XMapRequestEvent*)&event.xmaprequest);
      break;
    case MapNotify:
      handle_map_notify((XMapEvent*)&event.xmap);
      break;
    case UnmapNotify:
      handle_unmap_notify((XUnmapEvent*)&event.xunmap);
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
      handle_key_press((XKeyEvent*)&event.xkey);
      break;
    case KeyRelease:
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
    case ReparentNotify:
      handle_reparent((XReparentEvent*)&event.xreparent);
      break;
    }
  }
}

int main(int argc, char** argv) {
  struct sigaction act;
  act.sa_handler = timer_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  sigaction(SIGALRM, &act, NULL);

  dsp = XOpenDisplay(NULL);
  if (!dsp) {
    fatal("could not open display");
  }

  XSetErrorHandler(error_handler);

  root = XDefaultRootWindow(dsp);
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

  col.red = 20000;
  col.green = 20000;
  col.blue = 40000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  focused_colour = col;

  col.red = 30000;
  col.green = 30000;
  col.blue = 30000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  unfocused_colour = col;

  col.red = 0;
  col.green = 0;
  col.blue = 60000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  switching_colour = col;

  buffer_init(&clients, 500, sizeof(Client));
  buffer_init(&window_focus_history, 500, sizeof(Window));

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

  for (unsigned int i = 0; i < sizeof(keys) / sizeof(Key); i++) {
    Key k = keys[i];
    KeyCode kc = XKeysymToKeycode(dsp, k.sym);

    Key* kp = &keys[i];
    kp->kc = kc;

    XGrabKey(dsp, kc, k.mods, root, False, GrabModeAsync, GrabModeAsync);
  }

  kc_modl = XKeysymToKeycode(dsp, MODL);
  kc_modr = XKeysymToKeycode(dsp, MODR);

  XSelectInput(dsp, root, SubstructureRedirectMask | SubstructureNotifyMask);

  int xfd = ConnectionNumber(dsp);

  unsigned int ticker = 0;
  for (;;) {
    if (timer_expired) {
      timer_expired = 0;
      timer_has_expired();
    }

    handle_xevents();

    // todo - what to do about this gap here?
    // ie: XPending returns nothing, then before we start polling,
    // X writes a new event.

    // sleep, but not if there's incoming data
    struct pollfd fds[1];
    fds[0].fd = xfd;
    fds[0].events = POLLIN;
    poll(fds, 1, 100);

    // draw dots for some breathing room in the log
    if (++ticker % 100 == 0) info(".");
  }
}

#include "buffer.h"
#include "snap.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <poll.h>
#include <signal.h>

#define SNAP_DIST 30

// what fraction of window width/height do edge handles occupy?
#define HANDLE_FRAC 0.2

#define BORDER_WIDTH 4
#define BORDER_GAP 4
// todo screen gap

#define MODMASK Mod4Mask
#define MODL XK_Super_L
#define MODR XK_Super_R

//#define MODMASK Mod1Mask
//#define MODL XK_Alt_L
//#define MODR XK_Alt_R

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

  unsigned int focus_time;
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
      p.data = c;
      p.index = i;
    }
  }
  return p;
}

// todo pass around a context?
Display *dsp;
Window root;
XColor focused, unfocused, switching;
unsigned int screen_width, screen_height;
KeyCode key_m, key_v, key_h, key_esc, key_d, key_modl, key_modr;

Window last_focused_window = 0;

// we'll set this to 1 when we start cycling through windows.
// when timer expires, we'll set it back to 0 and update the window's focus time.
char transient_switching = 0;

// flag indicating timer has expired
char timer_expired = 0;

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
  if (st) {
    c.current_bounds.x = attr.x;
    c.current_bounds.y = attr.y;
    c.current_bounds.w = attr.width;
    c.current_bounds.h = attr.height;
  } else {
    warn("failed to get initial window attributes for %x", win);
    c.current_bounds.x = -1;
    c.current_bounds.y = -1;
    c.current_bounds.w = -1;
    c.current_bounds.h = -1;
  }

  c.max_state = MAX_NONE;
  c.focus_time = 0;

  buffer_add(&clients, &c);

  XSetWindowBorderWidth(dsp, win, BORDER_WIDTH);
  XSetWindowBorder(dsp, win, unfocused.pixel);
  XSelectInput(dsp, win, EnterWindowMask | FocusChangeMask);

  fine("added new window: %x", win);
}

void handle_map_request(XMapRequestEvent* event) {
  Window win = event->window;
  fine("map request for window: %x", win);
  manage_new_window(win);
  XMapWindow(dsp, win);
}

void handle_enter_notify(XCrossingEvent* event) {
  Window win = event->window;
  long t = event->time;
  fine("changing focus on enter-notify to window %x", win);
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

void track_focus_change(Window win) {
    XSetWindowBorder(dsp, win, focused.pixel);

    // increment all focus counters
    for (unsigned int i = 0; i < clients.length; i++) {
      Client *c = buffer_get(&clients, i);
      c->focus_time++;
    }

    // current focus gets 0
    Client *c = clients_find(win).data;
    if (!c) {
      info("no client for newly focused window: %x", win);
      return;
    }

    c->focus_time = 0;
}

void timer_handler(int signal) {
  timer_expired = 1;
}

void timer_has_expired() {
  fine("timer expired");
  transient_switching = 0;
  track_focus_change(last_focused_window);
}

// todo this is a disaster of design
void switch_next_window() {
  Client *current = clients_find(last_focused_window).data;
  if (!current) {
    info("no client for last focused window: %x", last_focused_window);
    return;
  }

  unsigned int ct = current->focus_time;

  // find the oldest client
  unsigned int oldest = 0;
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    unsigned int ft = c->focus_time;
    if (ft > oldest) oldest = ft;
  }

  // find the most recently focused client
  unsigned int t = oldest + 1;
  Client *next = NULL;
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    if (c == current) {
      continue;
    }
    unsigned int ft = c->focus_time;
    if (ft > ct && ft < t) {
      t = ft;
      next = c;
    }
  }

  if (!next) {
    info("wrap");
    t = oldest;
    for (unsigned int i = 0; i < clients.length; i++) {
      Client *c = buffer_get(&clients, i);
      unsigned int ft = c->focus_time;
      if (ft < t) {
        t = ft;
        next = c;
      }
    }
  }

  assert(next);

  Window win = next->win;
  fine("setting focus to %x", win);
  XRaiseWindow(dsp, win);
  XSetInputFocus(dsp, win, RevertToParent, CurrentTime);
}

void switch_windows() {
  if (!transient_switching) {
    transient_switching = 1;
  }

  // reset the timer
  struct itimerval t;
  t.it_value.tv_sec = 0;
  t.it_value.tv_usec = 600000;
  t.it_interval.tv_sec = 0;
  t.it_interval.tv_usec = 0;
  setitimer(ITIMER_REAL, &t, NULL);

  switch_next_window();
}

void handle_key_press(XKeyEvent *event) {
  KeyCode kc = event->keycode;
  if (kc == key_modl || kc == key_modr) {
    prime_mod = 1;
  } else {
    prime_mod = 0;
  }
}

void log_debug() {
  info("--- debug ---");
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    info("%15x %3d", c->win, c->focus_time);
  }
  info("------");
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
  } else if (kc == key_esc) {
    XLowerWindow(dsp, win);
  } else if (kc == key_d) {
    log_debug();
  } else if (prime_mod && (kc == key_modl || kc == key_modr)) {
    switch_windows();
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

void handle_focus_in(XFocusChangeEvent* event) {
  if (event->mode == NotifyGrab) {
    fine("discard grab focus-in");
    return;
  }

  Window win = event->window;
  last_focused_window = win;
  fine("focus in for window %x", win);

  if (transient_switching) {
    XSetWindowBorder(dsp, win, switching.pixel);
  } else {
    track_focus_change(win);
  }
}

void handle_focus_out(XFocusChangeEvent* event) {
  if (event->mode == NotifyGrab) {
    fine("discard grab focus-out");
    return;
  }

  Window win = event->window;
  XSetWindowBorder(dsp, win, unfocused.pixel);
  fine("focus out for window %x", win);
}

void handle_configure(XConfigureEvent* event) {
  Window win = event->window;
  int x = event->x;
  int y = event->y;
  int w = event->width;
  int h = event->height;
  Client* c = clients_find(win).data;
  if (!c) {
    info("handle_configure - no client for window %x", win);
    return;
  }
  c->current_bounds.x = x;
  c->current_bounds.y = y;
  c->current_bounds.w = w;
  c->current_bounds.h = h;
  fine("new position for window %x is [%d %d %d %d]", win, x, y, w, h);
}

void remove_window(Window win) {
  PI p = clients_find(win);
  if (!p.data) {
    warn("no client for window while removing: %x", win);
    return;
  }
  buffer_remove(&clients, p.index);
  fine("destroyed window %x", win);
}

void handle_destroy(XDestroyWindowEvent* event) {
  remove_window(event->window);
}

int error_handler(Display *dsp, XErrorEvent *event) {
  warn("X error");
  char buffer[128];
  XGetErrorText(dsp, event->error_code, buffer, 128);
  // todo malloc a string and log in one go, don't be a wuss
  warn("%d %d %s", event->request_code, event->minor_code, buffer);
  return 0;
}

void handle_reparent(XReparentEvent *event) {
  Window win = event->window;
  if (event->parent != root) {
    info("removing client after reparent: %x", win);
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

  col.red = 40000;
  col.green = 0;
  col.blue = 0;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  focused = col;

  col.red = 30000;
  col.green = 30000;
  col.blue = 30000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  unfocused = col;

  col.red = 65535;
  col.green = 0;
  col.blue = 0;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  switching = col;

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
  XGrabKey(dsp, key_m, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  key_v = XKeysymToKeycode(dsp, XK_V);
  XGrabKey(dsp, key_v, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  key_h = XKeysymToKeycode(dsp, XK_H);
  XGrabKey(dsp, key_h, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  key_esc = XKeysymToKeycode(dsp, XK_Escape);
  XGrabKey(dsp, key_esc, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  key_d = XKeysymToKeycode(dsp, XK_D);
  XGrabKey(dsp, key_d, MODMASK, root, False, GrabModeAsync, GrabModeAsync);

  // todo sync??
  // looks like pressing super first moves focus off the current win

  key_modl = XKeysymToKeycode(dsp, MODL);
  XGrabKey(dsp, key_modl, 0, root, False, GrabModeAsync, GrabModeAsync);

  key_modr = XKeysymToKeycode(dsp, MODR);
  XGrabKey(dsp, key_modr, 0, root, False, GrabModeAsync, GrabModeAsync);

  XSelectInput(dsp, root, SubstructureRedirectMask | SubstructureNotifyMask);

  int xfd = ConnectionNumber(dsp);

  unsigned int ticker = 0;
  for (;;) {
    if (timer_expired) {
      timer_expired = 0;
      timer_has_expired();
    }

    handle_xevents();

    // sleep, but not if there's incoming data
    struct pollfd fds[1];
    fds[0].fd = xfd;
    fds[0].events = POLLIN;
    poll(fds, 1, 100);

    // draw dots for some breathing room in the log
    if (ticker % 100 == 0) info(".");
    ticker++;
  }
}

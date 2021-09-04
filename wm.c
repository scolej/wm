#include <X11/Xlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "snap.h"

#define BORDER_WIDTH 2
#define BORDER_GAP 3

// Fixed number of clients. Could be better; should be fine.
#define MAX_CLIENTS 1000

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

void fine(char* msg, ...) {
  // todo
}

typedef struct {
  int x, y, w, h;
} Rectangle;

typedef struct {
  Window win;
  Rectangle bounds;
} Client;
// todo
// last focus time

Client clients[MAX_CLIENTS];
int last_client_index = -1;

void clients_init() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    clients[i].win = -1;
  }
}

// todo i'm sure using -1 as an 'empty signifier' is a bad idea

// Finds the next free slot.
Client* clients_next_available() {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    Client* c = &clients[i];
    if (c->win == -1) {
       last_client_index = i > last_client_index ? i : last_client_index;
       return c;
    }
  }
  return NULL;
}

void clients_remove(Window w) {
  for (int i = 0; i <= last_client_index; i++) {
    Client* c = &clients[i];
    if (c->win == w) {
      c->win = -1;
      if (i == last_client_index) {
        last_client_index--;
      }
      return;
    }
  }
}

// Finds a client by the window it represents.
Client* clients_find(Window w) {
  for (int i = 0; i <= last_client_index; i++) {
    if (clients[i].win == w) {
      info("found client for window %d at index %d", w, i);
      return &clients[i];
    }
  }
  return NULL;
}

// Starting from client at given index, find the next defined client.
Client* clients_next(int* first) {
  int i = *first + 1;
  info("looking for clients starting from %d", i);
  while (i <= last_client_index) {
    Client* c = &clients[i];
    if (c->win != -1) {
      *first = i;
      return c;
    }
    i++;
  }
  return NULL;
}

unsigned int clients_count() {
  info("counting clients");
  int i = -1;
  while (clients_next(&i));
  int n = i + 1;
  info("there are %d clients", n);
  return n;
}

// todo pass around a context?
Display* dsp;
XColor red, grey;

void manage_new_window(Window win) {
  Client* c = clients_next_available();
  if (!c) {
    warn("too many clients, ignoring new window: %d", win);
    return;
  }

  // Init new client
  c->win = win;
  c->bounds.x = -1;
  c->bounds.y = -1;
  c->bounds.w = -1;
  c->bounds.h = -1;

  XSetWindowBorderWidth(dsp, win, BORDER_WIDTH);
  XSetWindowBorder(dsp, win, grey.pixel);
  XSelectInput(dsp, win, EnterWindowMask | FocusChangeMask);
  // todo is it necessary to repeat this on every map?

  info("new window: %d", win);
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

  XRaiseWindow(dsp, win);
}

void handle_button_release(XButtonEvent* event) {
  info("finish drag");
  drag_state.win = 0;
}

// Make snap lists for edges: lefts, rights, tops, bottoms. These are the
// values for each edge which we can snap to. The pointer written into
// lefts needs to be freed by the caller. The other pointers share this
// same memory. Returns the number of elements in each list.
unsigned int make_snap_lists(Client* skip, int** ls, int** rs, int** ts, int** bs) {
  unsigned int nc = clients_count() - 1;
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
  int ci = -1;
  Client* c;
  while ((c = clients_next(&ci))) {
    if (skip == c) {
      continue;
    }

    Rectangle rect = c->bounds;
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
  (*rs)[count] = 800;
  (*ts)[count] = 0;
  (*bs)[count] = 600;
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
  Client* c = clients_find(win);
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
  Client* c = clients_find(win);
  if (!c) {
    warn("we have no client for window %d", win);
    return;
  }
  c->bounds.x = x;
  c->bounds.y = y;
  c->bounds.w = w;
  c->bounds.h = h;
  info("new position for window %d is [%d %d %d %d]", win, x, y, w, h);
}

void handle_destroy(XDestroyWindowEvent* event) {
  Window win = event->window;
  clients_remove(win);
  info("destroyed window %d", win);
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

  col.red = 30000;
  col.green = 30000;
  col.blue = 30000;
  st = XAllocColor(dsp, cm, &col);
  if (!st) {
    fatal("could not allocate colour");
  }
  grey = col;

  clients_init();

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
  XFree(children);

  drag_state.win = 0;

  XGrabButton(dsp, 1, Mod1Mask, root, False,
              ButtonPressMask | ButtonReleaseMask | Button1MotionMask,
              GrabModeAsync, GrabModeAsync, None, None);

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

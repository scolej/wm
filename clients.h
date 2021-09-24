#ifndef CLIENTS_H
#define CLIENTS_H

#include <X11/Xlib.h>
#include "buffer.h"

typedef struct {
  int x, y, w, h;
} Rectangle;

typedef struct {
  Rectangle current_bounds;

  // bounds without any maximization applied.
  // only non-zero after a maximization has been applied.
  Rectangle orig_bounds;

  Window win;

  // maximization state
  char max_state;

  // flag to track border toggle state (via current border width)
  char border_width;
} Client;

// Pair of pointer and array index.
// todo smell?
typedef struct {
  void* data;
  unsigned int index;
} PI;

// todo hide these away
extern Buffer clients;
extern Buffer window_focus_history;

void clients_add(Client *c);
void clients_del(Window win);

PI clients_find(Window win);
PI clients_most_recent();

Window window_history_get(unsigned int i);

// raise/lower win to the top/back of the focus history list
void clients_focus_raise(Window win);
void clients_focus_lower(Window win);

#endif

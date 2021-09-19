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
} Client;

// Pair of pointer and array index.
// todo smell?
typedef struct {
  void* data;
  unsigned int index;
} PI;

extern Buffer clients;
extern Buffer window_focus_history;

void clients_add(Client *c);
void clients_del(Window win);

PI clients_find(Window win);
PI clients_most_recent();

#endif

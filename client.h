#ifndef CLIENT_H
#define CLIENT_H

#include <X11/Xlib.h>

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

  char* name;
} Client;

// Pair of pointer and array index.
// todo smell?
typedef struct {
  Client* data;
  unsigned int index;
} PI;

#endif

#include "clients.h"
#include <assert.h>
#include <stdio.h>

struct ClientBuffer clients;
struct WindowBuffer window_focus_history;

// Finds a client by the window it represents.
PI clients_find(Window win) {
  PI p = {
    .data = NULL,
    .index = 0,
  };
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = cb_get(&clients, i);
    if (c->win == win) {
      p.data = c;
      p.index = i;
    }
  }
  return p;
}

// Finds the client which was most recently focused.
PI clients_most_recent() {
  Window* w = wb_get(&window_focus_history, 0);
  return clients_find(*w);
}

void clients_add(Client* c) {
  assert(c);
  cb_add(&clients, c);
  wb_add(&window_focus_history, &c->win);
  assert(clients.length == window_focus_history.length);
}

void clients_del(Window win) {
  PI p = clients_find(win);
  if (!p.data) {
    return;
  }

  Client *c = p.data;
  XFree(c->name);

  cb_remove(&clients, p.index);

  for (unsigned int i = 0; i < window_focus_history.length; i++) {
    Window* w = wb_get(&window_focus_history, i);
    printf("trying to delete %lx, cand is %lx\n", win, *w);
    if (win == *w) {
      wb_remove(&window_focus_history, i);
      break;
    }
  }

  printf("%ld %ld\n", clients.length, window_focus_history.length);
  assert(clients.length == window_focus_history.length);
}

// todo - code-gen a typed interface onto the buffer
Window window_history_get(unsigned int i) {
  Window *w = wb_get(&window_focus_history, i);
  return *w;
}

void clients_focus_raise(Window win) {
  for (unsigned int i = 0; i < window_focus_history.length; i++) {
    Window w = window_history_get(i);
    if (win == w) {
      wb_bring_to_front(&window_focus_history, i);
      return;
    }
  }
}

void clients_focus_lower(Window win) {
  for (unsigned int i = 0; i < window_focus_history.length; i++) {
    Window w = window_history_get(i);
    if (win == w) {
      wb_send_to_back(&window_focus_history, i);
      return;
    }
  }
}

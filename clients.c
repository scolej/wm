#include "clients.h"
#include <assert.h>

Buffer clients;
Buffer window_focus_history;

// Finds a client by the window it represents.
PI clients_find(Window win) {
  PI p = {
    .data = NULL,
    .index = 0,
  };
  for (unsigned int i = 0; i < clients.length; i++) {
    Client *c = buffer_get(&clients, i);
    if (c->win == win) {
      p.data = c;
      p.index = i;
    }
  }
  return p;
}

// Finds the client which was most recently focused.
PI clients_most_recent() {
  Window* w = buffer_get(&window_focus_history, 0);
  return clients_find(*w);
}

void clients_add(Client* c) {
  assert(c);
  buffer_add(&clients, c);
  buffer_add(&window_focus_history, &c->win);
  assert(clients.length == window_focus_history.length);
}

void clients_del(Window win) {
  PI p = clients_find(win);
  if (!p.data) {
    return;
  }

  buffer_remove(&clients, p.index);

  for (unsigned int i = 0; i < window_focus_history.length; i++) {
    Window* w = buffer_get(&window_focus_history, i);
    if (win == *w) {
      buffer_remove(&window_focus_history, i);
      break;
    }
  }

  assert(clients.length == window_focus_history.length);
}

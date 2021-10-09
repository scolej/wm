#ifndef CLIENTS_H
#define CLIENTS_H

#include "clientbuffer.h"
#include "windowbuffer.h"
#include "client.h"
#include <X11/Xlib.h>

// todo hide these away
extern struct ClientBuffer clients;
extern struct WindowBuffer window_focus_history;

void clients_add(Client *c);
void clients_del(Window win);

PI clients_find(Window win);
PI clients_most_recent();

Window window_history_get(unsigned int i);

// raise/lower win to the top/back of the focus history list
void clients_focus_raise(Window win);
void clients_focus_lower(Window win);

#endif

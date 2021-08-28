#include <stdlib.h>

#include "iter.h"

void* iter_next(Iter* iter) {
  void* d = iter->data;
  void* next = NULL;
  if (d) {
    next = (iter->next)(iter);
  }
  return next;
}

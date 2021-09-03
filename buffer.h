#ifndef buffer_h
#define buffer_h

#include <stdlib.h>

// A buffer of fixed size elements with no ordering guarantees.
// Elements are free to shuffle around as they are added and deleted.
// Don't rely on them staying in the same place.

typedef struct {
  char* data;
  unsigned long capacity;
  unsigned long length;
  size_t elem_size;
} Buffer;

void buffer_init(Buffer* buf, unsigned long capacity, unsigned long elem_size);

void buffer_free(Buffer* buf);

void* buffer_get(Buffer* buf, unsigned long index);

void buffer_add(Buffer* buf, void* data);

void buffer_remove(Buffer* buf, unsigned long index);

#endif

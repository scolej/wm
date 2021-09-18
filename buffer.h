#ifndef buffer_h
#define buffer_h

#include <stdlib.h>

// A buffer of fixed size elements.

typedef struct {
  char* data;
  unsigned long capacity;
  unsigned long length;
  size_t elem_size;
} Buffer;

// allocate a new buffer with initial capaity and element size
void buffer_init(Buffer* buf, unsigned long capacity, size_t elem_size);

// free the storage backing the buffer
void buffer_free(Buffer* buf);

// gets a pointer to the element at a given index
void* buffer_get(Buffer* buf, unsigned long index);

// add a new element at the end
void buffer_add(Buffer* buf, void* data);

// remove the element at given index, and fill its place with the last element
void buffer_remove(Buffer* buf, unsigned long index);

// bring the element at given index to the front and push everything else back
void buffer_bring_to_front(Buffer* buf, unsigned long index);

#endif

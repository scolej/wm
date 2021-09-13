#include "buffer.h"
#include <assert.h>
#include <string.h>

// todo
// it seems this may be completely wrong
// aliasing & UB ???

void buffer_init(Buffer* buf, unsigned long capacity, unsigned long elem_size) {
  buf->length = 0;
  buf->capacity = capacity;
  buf->elem_size = elem_size;
  buf->data = malloc(elem_size * capacity);
}

void buffer_free(Buffer* buf) {
  free(buf->data);
  buf->data = NULL;
  buf->capacity = 0;
  buf->length = 0;
}

void* buffer_get(Buffer* buf, unsigned long index) {
  assert(index < buf->length);
  return &buf->data[index * buf->elem_size];
}

void buffer_add(Buffer* buf, void* data) {
  assert(buf->length < buf->capacity); // fixme growable
  size_t s = buf->elem_size;
  memcpy(&buf->data[buf->length++ * s], data, s);
}

void buffer_remove(Buffer* buf, unsigned long index) {
  unsigned long l = buf->length;
  size_t s = buf->elem_size;
  assert(index < l);
  memcpy(&buf->data[index * s], &buf->data[--l * s], s);
  buf->length = l;
}

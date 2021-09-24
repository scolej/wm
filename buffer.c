#include "buffer.h"
#include <assert.h>
#include <string.h>

// todo
// it seems this may be completely wrong
// aliasing & UB ???

void buffer_init(Buffer* buf, unsigned long capacity, size_t elem_size) {
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
  assert(index < buf->length);
  unsigned long last = buf->length - 1;
  size_t s = buf->elem_size;
  if (index != last) {
    memcpy(&buf->data[index * s], &buf->data[last * s], s);
  }
  buf->length = last;
}

// todo this can go faster for sure!
// probably only need 3 memcpys int total
void buffer_bring_to_front(Buffer* buf, unsigned long index) {
  unsigned int len = buf->length;
  assert(index < len);
  size_t elem_size = buf->elem_size;
  size_t total_bytes = elem_size * len;

  char* tmp = malloc(total_bytes);
  assert(tmp);
  memcpy(tmp, buf->data, total_bytes);

  unsigned int ii = 0;
  for (unsigned int i = 0; i < len; i++) {
    unsigned int to;
    if (i == index) {
      to = 0;
    } else {
      to = ++ii;
    }
    memcpy(&buf->data[elem_size * to], &tmp[i * elem_size], elem_size);
  }

  free(tmp);
}

// todo better
void buffer_send_to_back(Buffer* buf, unsigned long index) {
  unsigned int len = buf->length;
  assert(index < len);
  size_t elem_size = buf->elem_size;
  size_t total_bytes = elem_size * len;

  char* tmp = malloc(total_bytes);
  assert(tmp);
  memcpy(tmp, buf->data, total_bytes);

  unsigned int ii = 0;
  for (unsigned int i = 0; i < len; i++) {
    unsigned int to;
    if (i == index) {
      to = len - 1;
    } else {
      to = ii++;
    }
    memcpy(&buf->data[elem_size * to], &tmp[i * elem_size], elem_size);
  }

  free(tmp);
}

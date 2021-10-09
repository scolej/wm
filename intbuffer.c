#include "intbuffer.h"
#include <assert.h>
#include <string.h>

void ib_init(struct IntBuffer *buf, unsigned long capacity) {
  buf->length = 0;
  buf->capacity = capacity;
  buf->data = calloc(sizeof(int), capacity);
}

void ib_free(struct IntBuffer *buf) {
  free(buf->data);
  buf->data = NULL;
  buf->capacity = 0;
  buf->length = 0;
}

int* ib_get(struct IntBuffer *buf, unsigned long index) {
  assert(index < buf->length);
  return &buf->data[index];
}

void ib_add(struct IntBuffer *buf, int* data) {
  assert(buf->length < buf->capacity); // fixme growable
  memcpy(&buf->data[buf->length++], data, sizeof(int));
}

void ib_remove(struct IntBuffer *buf, unsigned long index) {
  assert(index < buf->length);
  unsigned long last = buf->length - 1;
  if (index != last) {
    memcpy(&buf->data[index], &buf->data[last], sizeof(int));
  }
  buf->length = last;
}

// todo this can go faster for sure!
// probably only need 3 memcpys int total
void ib_bring_to_front(struct IntBuffer *buf, unsigned long index) {
  unsigned int len = buf->length;
  assert(index < len);
  size_t elem_size = sizeof(int);
  size_t total_bytes = elem_size * len;

  char* tmp = calloc(elem_size, len);
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
    memcpy(&buf->data[to], &tmp[i], elem_size);
  }

  free(tmp);
}

// todo better
void ib_send_to_back(struct IntBuffer *buf, unsigned long index) {
  unsigned int len = buf->length;
  assert(index < len);
  size_t elem_size = sizeof(int);
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
    memcpy(&buf->data[to], &tmp[i], elem_size);
  }

  free(tmp);
}

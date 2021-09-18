#include "buffer.h"
#include <stdarg.h>
#include <stdio.h>

void msg(char* str) {
  printf("%s\n", str);
}

void assert_int(int expected, int actual) {
  if (expected != actual) {
    printf("expected %d, but got %d\n", expected, actual);
    exit(1);
  }
}

void basic() {
  msg("basic");

  Buffer buf;
  int x;

  buffer_init(&buf, 4, sizeof(int));

  x = 1;
  buffer_add(&buf, &x);
  x = 2;
  buffer_add(&buf, &x);
  x = 3;
  buffer_add(&buf, &x);
  x = 4;
  buffer_add(&buf, &x);

  assert_int(1, *(int*)(buffer_get(&buf, 0)));
  assert_int(2, *(int*)(buffer_get(&buf, 1)));
  assert_int(3, *(int*)(buffer_get(&buf, 2)));
  assert_int(4, *(int*)(buffer_get(&buf, 3)));
  assert_int(4, buf.length);

  // remove first element
  buffer_remove(&buf, 0);
  assert_int(4, *(int*)(buffer_get(&buf, 0)));
  assert_int(3, buf.length);

  // remove second element
  buffer_remove(&buf, 1);
  assert_int(3, *(int*)(buffer_get(&buf, 1)));
  assert_int(2, buf.length);

  buffer_free(&buf);
}

void remove_last() {
  msg("remove_last");

  Buffer buf;
  int x;

  buffer_init(&buf, 4, sizeof(int));
  x = 100;
  buffer_add(&buf, &x);
  x = 200;
  buffer_add(&buf, &x);

  buffer_remove(&buf, 1);
  assert_int(1, buf.length);
  assert_int(100, *(int*)(buffer_get(&buf, 0)));

  buffer_free(&buf);
}

// assert int elements in a buffer. caller must specify same number of
// elements as buffer actually contains.
void assert_buffer_elements(Buffer* buf, ...) {
  va_list args;
  va_start(args, buf);
  for (unsigned int i = 0; i < buf->length; i++) {
    int* actual = buffer_get(buf, i);
    assert_int(va_arg(args, int), *actual);
  }
  va_end(args);
}

void bring_to_front() {
  msg("bring_to_front");

  Buffer buf;
  int x;

  buffer_init(&buf, 4, sizeof(int));
  x = 100;
  buffer_add(&buf, &x);
  x = 200;
  buffer_add(&buf, &x);
  x = 300;
  buffer_add(&buf, &x);
  x = 400;
  buffer_add(&buf, &x);

  msg("el 0");
  buffer_bring_to_front(&buf, 0);
  assert_buffer_elements(&buf, 100, 200, 300, 400);

  msg("el 1");
  buffer_bring_to_front(&buf, 1);
  assert_buffer_elements(&buf, 200, 100, 300, 400);

  msg("el 2");
  buffer_bring_to_front(&buf, 2);
  assert_buffer_elements(&buf, 300, 200, 100, 400);

  msg("el 3");
  buffer_bring_to_front(&buf, 3);
  assert_buffer_elements(&buf, 400, 300, 200, 100);

  buffer_free(&buf);
}

int main(int argc, char** argv) {
  basic();
  remove_last();
  bring_to_front();
  msg("success!");
}

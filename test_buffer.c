#include "buffer.h"
#include <assert.h>
#include <stdio.h>

void assert_int(int expected, int actual) {
  if (expected != actual) {
    printf("expected %d, but got %d\n", expected, actual);
    exit(1);
  }
}

int main(int argc, char** argv) {
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
}

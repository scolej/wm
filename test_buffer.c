#include "windowbuffer.h"
#include <stdarg.h>
#include <stdio.h>

void msg(char* str) {
  printf("%s\n", str);
}

void assert_win(Window expected, Window actual) {
  if (expected != actual) {
    printf("expected %lx, but got %lx\n", expected, actual);
    exit(1);
  }
}

void assert_int(int expected, int actual) {
  if (expected != actual) {
    printf("expected %d, but got %d\n", expected, actual);
    exit(1);
  }
}

void basic() {
  msg("basic");

  struct WindowBuffer buf;
  Window x;

  wb_init(&buf, 4);

  x = 1;
  wb_add(&buf, &x);
  x = 2;
  wb_add(&buf, &x);
  x = 3;
  wb_add(&buf, &x);
  x = 4;
  wb_add(&buf, &x);

  assert_win(1, *(int*)(wb_get(&buf, 0)));
  assert_win(2, *(int*)(wb_get(&buf, 1)));
  assert_win(3, *(int*)(wb_get(&buf, 2)));
  assert_win(4, *(int*)(wb_get(&buf, 3)));
  assert_win(4, buf.length);

  // remove first element
  wb_remove(&buf, 0);
  assert_win(4, *(int*)(wb_get(&buf, 0)));
  assert_win(3, buf.length);

  // remove second element
  wb_remove(&buf, 1);
  assert_win(3, *(int*)(wb_get(&buf, 1)));
  assert_win(2, buf.length);

  wb_free(&buf);
}

void remove_last() {
  msg("remove_last");

  struct WindowBuffer buf;
  Window x;

  wb_init(&buf, 4);
  x = 100;
  wb_add(&buf, &x);
  x = 200;
  wb_add(&buf, &x);

  wb_remove(&buf, 1);
  assert_int(1, buf.length);
  assert_win(100, *(int*)(wb_get(&buf, 0)));

  wb_free(&buf);
}

// assert int elements in a buffer. caller must specify same number of
// elements as buffer actually contains.
void assert_wb_elements(struct WindowBuffer* buf, ...) {
  va_list args;
  va_start(args, buf);
  for (unsigned int i = 0; i < buf->length; i++) {
    Window* actual = wb_get(buf, i);
    assert_win(va_arg(args, Window), *actual);
  }
  va_end(args);
}

void bring_to_front() {
  msg("bring_to_front");

  struct WindowBuffer buf;
  Window x;

  wb_init(&buf, 4);
  x = 100;
  wb_add(&buf, &x);
  x = 200;
  wb_add(&buf, &x);
  x = 300;
  wb_add(&buf, &x);
  x = 400;
  wb_add(&buf, &x);

  msg("el 0");
  wb_bring_to_front(&buf, 0);
  assert_wb_elements(&buf, 100, 200, 300, 400);

  msg("el 1");
  wb_bring_to_front(&buf, 1);
  assert_wb_elements(&buf, 200, 100, 300, 400);

  msg("el 2");
  wb_bring_to_front(&buf, 2);
  assert_wb_elements(&buf, 300, 200, 100, 400);

  msg("el 3");
  wb_bring_to_front(&buf, 3);
  assert_wb_elements(&buf, 400, 300, 200, 100);

  wb_free(&buf);
}

void send_to_back() {
  msg("send_to_back");

  struct WindowBuffer buf;
  Window x;

  wb_init(&buf, 4);
  x = 100;
  wb_add(&buf, &x);
  x = 200;
  wb_add(&buf, &x);
  x = 300;
  wb_add(&buf, &x);
  x = 400;
  wb_add(&buf, &x);

  msg("el 0");
  wb_send_to_back(&buf, 0);
  assert_wb_elements(&buf, 200, 300, 400, 100);

  msg("el 1");
  wb_send_to_back(&buf, 1);
  assert_wb_elements(&buf, 200, 400, 100, 300);

  msg("el 2");
  wb_send_to_back(&buf, 2);
  assert_wb_elements(&buf, 200, 400, 300, 100);

  msg("el 3");
  wb_send_to_back(&buf, 3);
  assert_wb_elements(&buf, 200, 400, 300, 100);

  wb_free(&buf);
}

int main(int argc, char** argv) {
  basic();
  remove_last();
  bring_to_front();
  send_to_back();
  msg("success!");
}

#include "snap.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void assert_int(int expected, int actual) {
  if (expected != actual) {
    printf("expected %d, but got %d\n", expected, actual);
    exit(1);
  }
}

int main(int argc, char** argv) {
  int n = 5;
  int* xs = calloc(n, sizeof(int));
  xs[0] = 10;
  xs[1] = 20;
  xs[2] = 21;
  xs[3] = 30;
  xs[4] = 25;

  // no snap at all
  assert_int(50, snap(50, xs, n));

  // input is same as a snap value
  assert_int(10, snap(10, xs, n));

  // nominal snap
  assert_int(10, snap(11, xs, n));

  // snap to closest
  assert_int(20, snap(19, xs, n));
  assert_int(21, snap(21, xs, n));
}

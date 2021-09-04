#include "snap.h"
#include <stdlib.h>

#define SNAP_RANGE 15

int snap(int x, int* xs, int n) {
  unsigned int d = SNAP_RANGE;
  int r = x;
  for (int i = 0; i < n; i++) {
    int c = xs[i];
    unsigned int dd = abs(c - x);
    if (dd < d) {
      d = dd;
      r = c;
    }
  }
  return r;
}

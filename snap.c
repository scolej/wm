#include "snap.h"
#include <stdlib.h>

int snap(int x, int* xs, unsigned int n, unsigned int dist) {
  unsigned int d = dist;
  int r = x;
  for (unsigned int i = 0; i < n; i++) {
    int c = xs[i];
    unsigned int dd = abs(c - x);
    if (dd < d) {
      d = dd;
      r = c;
    }
  }
  return r;
}

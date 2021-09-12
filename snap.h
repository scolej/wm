#ifndef snap_h
#define snap_h

// Finds the value from XS (whose length is N) to which X should snap.
// dist is the snap vicinity.
int snap(int x, int* xs, unsigned int n, unsigned int dist);

#endif

#ifndef ITER_H
#define ITER_H

struct Iter {
  void* data;
  void* (*next)(struct Iter*);
};

typedef struct Iter Iter;

void* iter_next(Iter* iter);

#endif

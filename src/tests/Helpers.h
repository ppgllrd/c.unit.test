#if !defined(HELPERS_H)
#define HELPERS_H

#include <stddef.h>

struct X {
  int i;
  struct X* x;
};

struct Y {
  struct X* x; 
  size_t s;    
};

struct Y* _n(int a[], size_t s);
int _c(struct Y* y, struct Y* yy);

#endif // HELPERS_H
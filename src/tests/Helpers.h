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

struct Y* _n(int G[],size_t F);
void _p(char*H, size_t I, struct Y*A);
int _c(struct Y*F,struct Y*G);
int _v(struct Y*A, char*buf, size_t size);

#endif // HELPERS_H
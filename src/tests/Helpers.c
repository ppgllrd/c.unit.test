#include "test/unit/UnitTest.h"
#include "Helpers.h"

struct X* _x(int B,struct X*C) {
    struct X*A=malloc(sizeof(struct X));*A=(struct X){B,C};return A;
}

struct Y* _n(int G[],size_t F) {
    struct Y*A=malloc(sizeof(struct Y));struct X*B,*C;
    if(F){int*D=G+--F;B=C=_x(*D--,NULL);for(size_t E=F;E;--E)B=_x(*D--,B);F++,C->x=B;}
    *A=(struct Y){C,F};return A;
}

void _p(struct Y* y) {
    struct X*x=y->x;
    for(size_t i=0;i<y->s;i++) {
        printf("%d ",(x=x->x,x->i));
        fflush(stdout);
    }
}

int _c(struct Y*F,struct Y*G) {
    if(F->s^G->s)return 0;
    struct X*C=F->x,*D=G->x;size_t A=F->s,B=G->s;int E=1;
    if(A^B)return 0;
    while(A--){if(C->i^D->i){E=0;break;}C=C->x;D=D->x;}
    return E&&(C==F->x&&D==G->x);
}

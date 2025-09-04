/*============================================================================*/
/* Helper functions for CircularLinkedList                                    */
/* Pepe Gallardo, 2025                                                        */
/*============================================================================*/

#include "Helpers.h"

#define UNIT_TEST_MEMORY_TRACKING
#include "test/unit/UnitTest.h"

struct X* _x(int B,struct X*C){struct X*A=malloc(sizeof(struct X));*A=(struct X){B,C};return A;}
struct Y* _n(int G[],size_t F){struct Y*A=malloc(sizeof(struct Y));struct X*B,*C;if(F){int*D=G+--F;B=C=_x(*D--,NULL);for(size_t E=F;E;--E)B=_x(*D--,B);F++,C->x=B;}*A=(struct Y){C,F};return A;}
void _p(char*H,size_t I,struct Y*A){struct X*B=A->x;size_t C=0;int n=snprintf(H+C,I-C,"CircularLinkedList(");if(n<0||(size_t)n>=I-C)return;C+=n;for(size_t i=0;i<A->s;i++){n=snprintf(H+C,I-C,i==A->s-1?"%d":"%d,",(B=B->x,B->i));if(n<0||(size_t)n>=I-C)break;C+=n;}n=snprintf(H+C,I-C,")");}
int _c(struct Y*F,struct Y*G){if(F->s^G->s)return 0;struct X*C=F->x,*D=G->x;size_t A=F->s,B=G->s;int E=1;if(A^B)return 0;while(A--){if(C->i^D->i){E=0;break;}C=C->x;D=D->x;}return E&&(C==F->x&&D==G->x);}
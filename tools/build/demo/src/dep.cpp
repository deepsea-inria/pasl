
#include "dep.hpp"
#include <cstdio>

int f(int x) { 
#ifdef DEBUG
  printf("debug\n");
#else
  printf("not debug\n");
#endif
  return x; 
}  
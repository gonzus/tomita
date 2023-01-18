#include <stdio.h>
#include <string.h>
#include "util.h"
#include "mem.h"

void *Allocate(unsigned Bytes) {
  void* X = 0;
  MALLOC(void*, X, Bytes);
  if (X == 0) {
    printf("Out of memory.\n");
    exit(1);
  }
  return X;
}

void *Reallocate(void* P, unsigned Bytes) {
  void* X = P;
  REALLOC(void*, X, Bytes);
  if (X == 0) {
    printf("Out of memory.\n");
    exit(1);
  }
  P = X;
  return P;
}

const char *CopyS(const char *S) {
  char *NewS = (char *)Allocate(strlen(S) + 1);
  strcpy(NewS, S);
  return NewS;
}

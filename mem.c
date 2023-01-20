#include <stdio.h>
#include <string.h>
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

char *CopyB(const char *ptr, unsigned len) {
  char *NewS = (char *)Allocate(len);
  memcpy(NewS, ptr, len);
  return NewS;
}

char *CopyS(const char *S) {
  unsigned len = strlen(S);
  char *NewS = CopyB(S, len + 1);
  NewS[len] = '\0';
  return NewS;
}

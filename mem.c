#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mem.h"

void *Allocate(unsigned Bytes) {
  void *X = malloc(Bytes);
  if (X == 0) printf("Out of memory.\n"), exit(1);
  return X;
}

void *Reallocate(void *X, unsigned Bytes) {
  X = realloc(X, Bytes);
  if (X == 0) printf("Out of memory.\n"), exit(1);
  return X;
}

char *CopyS(char *S) {
  char *NewS = (char *)Allocate(strlen(S) + 1);
  strcpy(NewS, S); return NewS;
}

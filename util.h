#pragma once

#include <assert.h>
#include <stdlib.h>

// t: type; v: name of variable; s: size
#define MALLOC(t, v, s) \
  do { \
    assert(v == 0); \
    v = (t) malloc(s); \
    assert(v != 0); \
    memset(v, 0, s); \
  } while (0)

// t: type; v: name of variable; s: size
#define REALLOC(t, v, s) \
  do { \
    v = (t) realloc(v, s); \
    assert(v != 0); \
  } while (0)

// v: name of variable
#define FREE(v) \
  do { \
    /* assert(v != 0); */ \
    free(v); \
    v = 0; \
  } while (0)

void ERROR(int line, int* errors, const char *Format, ...);

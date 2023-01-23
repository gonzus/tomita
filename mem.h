#pragma once

#include <assert.h>
#include <stdlib.h>

// equivalent of: v = (T) malloc(s)
// v must be declared already
#define MALLOC_S(T, v, s) \
  do { \
    assert(v == 0); \
    if ((s) > 0) { \
      v = (T) malloc(s); \
      assert(v != 0); \
      memset(v, 0, s); \
    } \
  } while (0)

// equivalent of: v = (T*) malloc(n*sizeof(T))
// v must be declared already
#define MALLOC_N(T, v, n) \
  do { \
    MALLOC_S(T*, v, (n)*sizeof(T)); \
  } while (0)

// equivalent of: v = (T*) malloc(sizeof(T))
// v must be declared already
#define MALLOC(T, v) \
  do { \
    MALLOC_N(T, v, 1); \
  } while (0)

// equivalent of: v = (T) realloc(v, s)
// v must be declared already
#define REALLOC_S(T, v, s) \
  do { \
    if ((s) > 0) { \
      T tmp = (T) realloc(v, s); \
      assert(tmp != 0); \
      v = tmp; \
    } \
  } while (0)

// equivalent of: v = (T*) realloc(v, n*sizeof(T))
// v must be declared already
#define REALLOC(T, v, n) \
  do { \
    REALLOC_S(T*, v, (n)*sizeof(T)); \
  } while (0)

// v: name of variable
#define FREE(v) \
  do { \
    if ((v) != 0) { \
      free(v); \
      v = 0; \
    } \
  } while (0)

#define REF(x) ((x) ? (++((x)->Links), x)  : 0)

#define UNREF(x) \
  if (x) { \
    if ((x)->Links) {\
      --((x)->Links); \
      if (!(x)->Links) {\
        FREE(x); \
        x = 0; \
      } \
    } \
  }

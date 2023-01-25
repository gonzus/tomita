#pragma once

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// ===============================
// simple memory management macros
// ===============================

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


// ================================
// manage reference-counted structs
// ================================

// increment reference count and return variable
#define REF(x) ((x) ? (++((x)->ref_cnt), x)  : 0)

// decrement reference count and possibly free variable
#define UNREF(x) \
  if (x) { \
    if ((x)->ref_cnt) {\
      --((x)->ref_cnt); \
      if (!(x)->ref_cnt) {\
        FREE(x); \
        x = 0; \
      } \
    } \
  }


// ======================================
// manage arrays allocated with realloc()
// ======================================

// check if a table is full; if so, increase its size by extra elements
// assumes extra is a power of 2, so that it is easy to check for fullness
#define TABLE_CHECK_GROW(tab, cap, extra, T) \
  do { \
    if (((cap) & (extra-1)) == 0) { \
      LOG_DEBUG("GROW %s (%s) by %u from %u to %u", #tab, #T, extra, cap, cap+extra); \
      REALLOC(T, tab, (cap) + (extra)); \
    } \
  } while (0)

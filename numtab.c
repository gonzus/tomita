#include <assert.h>
#include <stdio.h>
#include "mem.h"
#include "tomita.h"
#include "numtab.h"

static unsigned triple32(unsigned x);

NumTab* numtab_create(void) {
  NumTab* numtab = 0;
  MALLOC(NumTab, numtab);
  return numtab;
}

void numtab_destroy(NumTab* numtab) {
  numtab_clear(numtab);
  FREE(numtab);
}

void numtab_clear(NumTab* numtab) {
  for (unsigned h = 0; h < NUMTAB_HASH_MAX; ++h) {
    for (Number* num = numtab->table[h]; num != 0; ) {
      Number* tmp = num;
      num = num->next;
      FREE(tmp);
    }
    numtab->table[h] = 0;
  }
}

void numtab_show(NumTab* numtab) {
  printf("%c%c numtab\n", FORMAT_COMMENT, FORMAT_COMMENT);
  printf("table size: %u\n", NUMTAB_HASH_MAX);
  for (unsigned h = 0; h < NUMTAB_HASH_MAX; ++h) {
    Number* bucket = numtab->table[h];
    if (!bucket) continue;
    printf("== bucket %u ==\n", h);
    for (Number* num = numtab->table[h]; num != 0; num = num->next) {
      printf("value %u => %p\n", num->val, num->ptr);
    }
  }
}

Number* numtab_lookup(NumTab* numtab, unsigned val, unsigned char insert) {
  Number* num = 0;
  unsigned char h = triple32(val);

  // try to locate first
  for (num = numtab->table[h]; num != 0; num = num->next) {
    if (num->val != val) continue;
    return num;
  }

  // not found
  if (!insert) return 0;

  // must create and chain
  MALLOC(Number, num);
  num->val = val;
  num->next = numtab->table[h];
  numtab->table[h] = num;
  return num;
}

// https://nullprogram.com/blog/2018/07/31/
static unsigned triple32(unsigned x) {
    x ^= x >> 17;
    x *= 0xed5ad4bbU;
    x ^= x >> 11;
    x *= 0xac4c1b51U;
    x ^= x >> 15;
    x *= 0x31848babU;
    x ^= x >> 14;
    return x;
}

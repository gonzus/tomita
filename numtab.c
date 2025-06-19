#include <assert.h>
#include <stdio.h>
#include "mem.h"
#include "tomita.h"
#include "numtab.h"

static unsigned murmurhash32_mix32(unsigned x);

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
  unsigned char h = murmurhash32_mix32(val);

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

// TODO: (as in every project) use a better hash function?
static unsigned murmurhash32_mix32(unsigned x) {
    x ^= x >> 16;
    x *= 0x85ebca6bU;
    x ^= x >> 13;
    x *= 0xc2b2ae35U;
    x ^= x >> 16;
    return x;
}

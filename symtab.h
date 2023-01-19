#pragma once

#include "symbol.h"

// TODO: make hash table growable?
#define HASH_MAX 0x100

typedef struct SymTab {
  Symbol* table[HASH_MAX];
} SymTab;

SymTab* symtab_create(void);
void symtab_destroy(SymTab* symtab);
int symtab_lookup(SymTab* symtab, Slice name, unsigned char literal, Symbol** sym);

#pragma once

#include <stdio.h>
#include "symbol.h"

// TODO: make hash table growable?
#define HASH_MAX 0x100

// a symbol (hash) table
typedef struct SymTab {
  unsigned counter;
  Symbol* table[HASH_MAX];
} SymTab;

// Create an empty symbol table.
SymTab* symtab_create(void);

// Destroy a symbol table created with symtab_create().
void symtab_destroy(SymTab* symtab);

// Look up a symbol with given name and literal, and create it if not found.
// Return 1 if the symbol was created anew, 0 if it was alredy there.
int symtab_lookup(SymTab* symtab, Slice name, unsigned char literal, Symbol** sym);

void symtab_print(SymTab* symtab, FILE* fp);

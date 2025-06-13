#pragma once

#include "buffer.h"
#include "symbol.h"

// TODO: make hash table growable?
enum {
  SYMTAB_HASH_MAX = 0x100,
};

// a symbol (hash) table
typedef struct SymTab {
  unsigned symbol_counter;        // to create sequential indexes for symbols
  unsigned rules_counter;         // to create sequential indexes for rulesets
  Symbol* table[SYMTAB_HASH_MAX]; // the actual table
  Symbol* first;                  // the first symbol seen
  Symbol* last;                   // the last symbol seen
} SymTab;

// Create an empty symbol table.
SymTab* symtab_create(void);

// Destroy a symbol table created with symtab_create().
void symtab_destroy(SymTab* symtab);

// Clear all contents of a symbol table -- leave it as just created.
void symtab_clear(SymTab* symtab);

// Look up a symbol with given name and literal, and create it if not found and we were told.
// Return the found / created symbol, or null if not found and not created.
Symbol* symtab_lookup(SymTab* symtab, Slice name, unsigned char literal, unsigned char insert);

Symbol* find_symbol_by_index(SymTab* symtab, unsigned index);

// Load a symbol table from a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned symtab_load_from_slice(SymTab* symtab, Slice* text);

// Save a symbol table into a buffer.
// Format for saved contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned symtab_save_to_buffer(SymTab* symtab, Buffer* b);

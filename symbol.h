#pragma once

#include "slice.h"

// The rules of a symbol point to other symbols.
// These pointers (and their containing array) are dynamically allocated,
// but the symbols themselves just live in the symbol table.
typedef struct Symbol {
  Slice name;              // name for symbol
  unsigned char literal;   // not sure...
  unsigned char defined;   // not sure...
  unsigned index;          // sequential number
  unsigned rule_count;     // how many rules we have in rules
  struct Symbol*** rules;  // triple pointer?!?
  struct Symbol* next;     // for symbol table chaining
  struct Symbol* tail;     // not sure...
} Symbol;

Symbol* symbol_create(Slice name, unsigned literal);
void symbol_destroy(Symbol* symbol);
void symbol_insert_rule(Symbol* symbol, Symbol** SymBuf, Symbol** SymP);

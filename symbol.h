#pragma once

#include "slice.h"

// The rules of a symbol point to other symbols.
// These pointers (and their containing array) are dynamically allocated,
// but the symbols themselves just live in the symbol table.
// Therefore, we can simply delete the table at the end.

// a Symbol in the grammar
typedef struct Symbol {
  Slice name;              // name for symbol
  unsigned char literal;   // is this a terminal (literal) or a non-terminal symbol?
  unsigned char defined;   // was there a definition for this symbol?
  unsigned index;          // sequential number
  unsigned rule_count;     // how many rules we have in rules
  struct Symbol*** rules;  // list of rules in right-hand side -- triple pointer!
  struct Symbol* nxt_hash; // for symbol table chaining
  struct Symbol* nxt_list; // for linked list of all symbols
} Symbol;

// Create a symbol given its name and whether is is a literal.
Symbol* symbol_create(Slice name, unsigned literal);

// Destroy a symbol created with symbol_create().
void symbol_destroy(Symbol* symbol);

// Insert the right-hand side rule that defines a (non-terminal) symbol.
void symbol_insert_rule(Symbol* symbol, Symbol** SymBuf, Symbol** SymP);

#pragma once

#include "buffer.h"

typedef struct RuleSet {
  unsigned index;          // sequential ruleset number
  struct Symbol** rules;   // null-terminated list of symbols in right-hand side
} RuleSet;

// The rules of a symbol point to other symbols.
// These pointers (and their containing array) are dynamically allocated,
// but the symbols themselves just live in the symbol table.
// Therefore, we can simply delete the table at the end.

// a Symbol in the grammar
typedef struct Symbol {
  unsigned index;          // sequential symbol number
  Slice name;              // name for symbol
  unsigned char literal;   // is this a terminal (literal) or a non-terminal symbol?
  unsigned char defined;   // was there a definition for this symbol?
  RuleSet* rs_table;       // table of RuleSets
  unsigned rs_cap;         //   capacity of table
  struct Symbol* nxt_hash; // for symbol table chaining
  struct Symbol* nxt_list; // for linked list of all symbols
} Symbol;

// Create a symbol given its name and whether is is a literal.
Symbol* symbol_create(Slice name, unsigned literal, unsigned* counter);

// Destroy a symbol created with symbol_create().
void symbol_destroy(Symbol* symbol);

// Insert the right-hand side rule that defines a (non-terminal) symbol.
void symbol_insert_rule(Symbol* symbol, Symbol** SymBuf, Symbol** SymP, unsigned* counter, unsigned index);

// Save the symbol's definitions into a buffer.
void symbol_save_definition(Symbol* symbol, Buffer* b);

// Save the symbol's rules into a buffer.
void symbol_save_rules(Symbol* symbol, Buffer* b);

// Find a ruleset for a symbol given its index.
RuleSet* find_ruleset_by_index(Symbol* symbol, unsigned index);

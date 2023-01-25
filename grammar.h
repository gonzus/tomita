#pragma once

#include "slice.h"
#include "symtab.h"

// a grammar, including a symbol table
typedef struct Grammar {
  SymTab* symtab;            // the symbol table
  Symbol* start;             // the first (root) symbol
  Symbol* first;             // TODO
  Symbol* last;              // TODO
} Grammar;

// Create a grammar from a given textual definition.
// Format for text is (almost) yacc-compatible.
Grammar* grammar_create(Slice text);

// Destroy a grammar created with grammar_create().
void grammar_destroy(Grammar* grammar);

// Check if a grammar is usable.
unsigned grammar_check(Grammar* grammar);

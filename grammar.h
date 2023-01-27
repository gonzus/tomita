#pragma once

#include "slice.h"
#include "buffer.h"
#include "symtab.h"

// a grammar, including a symbol table
typedef struct Grammar {
  Buffer source;
  SymTab* symtab;            // the symbol table
  Symbol* start;             // the start symbol
  Symbol* first;             // the first symbol seen
  Symbol* last;              // the last symbol seen
} Grammar;

// Create an empty grammar.
Grammar* grammar_create(void);

// Destroy a grammar created with grammar_create().
void grammar_destroy(Grammar* grammar);

// Print a grammar in a human-readable format.
void grammar_show(Grammar* grammar);

// Build a grammar from a given textual definition.
// Format for text is (almost) yacc-compatible.
// Return number of errors found (so 0 => ok)
unsigned grammar_build_from_text(Grammar* grammar, Slice source);

// Load a grammar from a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_load_from_path(Grammar* grammar, const char* path);

// Save a grammar into a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_save_to_path(Grammar* grammar, const char* path);

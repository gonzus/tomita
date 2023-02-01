#pragma once

#include <stdio.h>
#include "slice.h"
#include "buffer.h"
#include "symtab.h"

// a grammar, including a symbol table
typedef struct Grammar {
  Buffer source;             // copy of the source
  SymTab* symtab;            // the symbol table
  Symbol* start;             // the start symbol
} Grammar;

// Create an empty grammar.
Grammar* grammar_create(SymTab* symtab);

// Destroy a grammar created with grammar_create().
void grammar_destroy(Grammar* grammar);

// Print a grammar in a human-readable format.
void grammar_show(Grammar* grammar);

// Build a grammar from a given textual definition.
// Format for text is (almost) yacc-compatible.
// Return number of errors found (so 0 => ok)
unsigned grammar_build_from_text(Grammar* grammar, Slice source);

// Load a grammar from a slice.
// Format for slice contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_load_from_slice(Grammar* grammar, Slice source);

// Save a grammar into a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_save_to_stream(Grammar* grammar, FILE* fp);

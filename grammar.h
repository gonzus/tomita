#pragma once

#include "buffer.h"

// a grammar, including a symbol table
typedef struct Grammar {
  Buffer source;             // copy of the source
  struct SymTab* symtab;            // the symbol table
  struct Symbol* start;             // the start symbol
} Grammar;

// Create an empty grammar.
Grammar* grammar_create(struct SymTab* symtab);

// Destroy a grammar created with grammar_create().
void grammar_destroy(Grammar* grammar);

// Clear all contents of a grammar -- leave it as just created.
void grammar_clear(Grammar* grammar);

// Print a grammar in a human-readable format.
void grammar_show(Grammar* grammar);

// Compile a grammar from a given textual source.
// Format for source is (almost) yacc-compatible.
// Return number of errors found (so 0 => ok)
unsigned grammar_compile_from_slice(Grammar* grammar, Slice source);

// Load a compiled grammar from a slice.
// Format for loaded contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_load_from_slice(Grammar* grammar, Slice source);

// Save a compiled grammar into a buffer.
// Format for saved contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned grammar_save_to_buffer(Grammar* grammar, Buffer* b);

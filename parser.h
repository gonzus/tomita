#pragma once

#include <stdio.h>
#include "buffer.h"
#include "symtab.h"
#include "grammar.h"

// a Shift action
struct Shift {
  Symbol* symbol;            // the symbol causing the shift
  unsigned state;            // state to change to
};

// a Reduce action
struct Reduce {
  Symbol* lhs;               // the left-hand side of the rule being reduced
  RuleSet rs;                // the right-hand side ruleset
};

// a State in the parsing table
struct State {
  unsigned char final;       // is this a final state?
  struct Shift* ss_table;    // table of Shift actions
  unsigned ss_cap;           //   capacity of table
  struct Reduce* rr_table;   // table of Reduce actions
  unsigned rr_cap;           //   capacity of table
  Symbol** er_table;         // table of epsilon reductions
  unsigned er_cap;           //   capacity of table
};

// a Parser
typedef struct Parser {
  Buffer source;             // copy of the source
  SymTab* symtab;            // the symbol table
  struct State* state_table; // state table
  unsigned state_cap;        // capacity of state and items tables
} Parser;


// Create a parser that uses a given SymTab.
Parser* parser_create(SymTab* symtab);

// Destroy a parser created with parser_create().
void parser_destroy(Parser* parser);

// Print a parser in a human-readable format.
void parser_show(Parser* parser);

// Build a parser from a given grammar.
// Return number of errors found (so 0 => ok)
unsigned parser_build_from_grammar(Parser* parser, Grammar* grammar);

// Load a parser from a slice.
// Format for slice contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned parser_load_from_slice(Parser* parser, Slice source);

// Save a parser into a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned parser_save_to_stream(Parser* parser, FILE* fp);

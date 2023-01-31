#pragma once

#include <stdio.h>
#include "grammar.h"

// a Shift action
struct Shift {
  Symbol* symbol;            // the symbol causing the shift
  unsigned state;            // state to change to
};

// a Reduce action
struct Reduce {
  Symbol* lhs;               // the left-hand side of the rule being reduced
  Symbol** rhs;              // the right-hand side (list of Symbols)
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
  Grammar* grammar;          // grammar used to generate parser
  struct State* state_table; // state table
  unsigned table_cap;        // capacity of state and items tables
} Parser;


// Create a parser from a given grammar.
Parser* parser_create(Grammar* grammar);

// Destroy a parser created with parser_create().
void parser_destroy(Parser* parser);

// Print a parser in a human-readable format.
void parser_show(Parser* parser);

// Save a parser into a given path.
// Format for file contents are "proprietary".
// Return number of errors found (so 0 => ok)
unsigned parser_save_to_stream(Parser* parser, FILE* fp);

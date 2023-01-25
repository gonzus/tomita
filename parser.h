#pragma once

#include "grammar.h"

// a reference-counted Item
// TODO: for what purposes?
struct Item {
  Symbol* lhs;               // left-hand side symbol
  Symbol** rhs_table;        // table of symbols in right-hand side
  Symbol** rhs_pos;          //   pointer to "current" symbol
  unsigned ref_cnt;          // reference count
};

// a list of Item pointers, with a Symbol
// TODO: for what purposes?
struct Items {
  Symbol* Pre;               // TODO
  struct Item** item_table;  // Item table
  unsigned item_cap;         //   capacity of table
};

// a Shift action
struct Shift {
  Symbol* symbol;            // the symbol causing the shift
  int state;                 // state to change to
};

// a Reduce action
struct Reduce {
  Symbol* lhs;               // the left-hand side of the rule being reduced
  Symbol** rhs;              // the right-hand side (list of Symbols)
};

// a State in the parsing table
struct State {
  unsigned char final;       // is this a final state?
  Symbol** er_table;         // table of epsilon reductions
  unsigned er_cap;           //   capacity of table
  struct Reduce* rr_table;   // table of Reduce actions
  unsigned rr_cap;           //   capacity of table
  struct Shift* ss_table;    // table of Shift actions
  unsigned ss_cap;           //   capacity of table
};

// a Parser
typedef struct Parser {
  Grammar* grammar;          // grammar used to generate parser
  struct State* state_table; // state table
  struct Items* items_table; // items table
  unsigned table_cap;           // capacity of state and items tables
} Parser;


// Create a parser from a given grammar.
Parser* parser_create(Grammar* grammar);

// Destroy a parser created with parser_create().
void parser_destroy(Parser* parser);

// Print a parser in a human-readable format.
void parser_show(Parser* parser);

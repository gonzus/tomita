#pragma once

#include "symbol.h"
#include "lexer.h"
#include "parser.h"

typedef struct Forest {
  LexerI* lexer;

  Parser* parser;            // the parser used to create this parse forest
  int prepared;              // is this forest prepared, or was it cleaned up?
  unsigned position;         // sequential position value

  struct Node* node_table;   // node table
  unsigned node_cap;         //   capacity of table
  unsigned node_pos;         //   "current" element

  struct Vertex* vert_table; // vertex table
  unsigned vert_cap;         //   capacity of table
  unsigned vert_pos;         //   "current" element

  struct Path* path_table;   // path table
  unsigned path_cap;         //   capacity of table

  struct RRed* rr_table;     // regular reductions table (non-empty right-hand side)
  unsigned rr_cap;           //   capacity of table
  unsigned rr_pos;           //   "current" element

  struct ERed* er_table;     // empty reductionsA table (right-hand side empty)
  unsigned ee_cap;           //   capacity of table
  unsigned ee_pos;           //   "current" element
} Forest;

Forest* forest_create(LexerI* lexer, Parser* parser);
void forest_destroy(Forest* forest);

struct Node* forest_parse(Forest* forest, Slice text);

void forest_show(Forest* forest);
void forest_show_node(Forest* forest, struct Node* node);
void forest_show_stack(Forest* forest);

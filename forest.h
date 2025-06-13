#pragma once

#include "slice.h"

// a reference-counted subnode, part of a node's branch
struct Subnode {
  unsigned Size;
  unsigned Cur;
  unsigned ref_cnt;          // reference count
  struct Subnode* next;      // link to next Subnode
};

// a node of a parse forest; it points to all possible parsed branches
struct Node {
  struct Symbol* symbol;     // symbol pointed to by this node
  unsigned Start;
  unsigned Size;
  struct Subnode** sub_table;// table of branches for node
  unsigned sub_cap;          //   capacity of table
};

// a parse forest, which contains one or more parse trees
typedef struct Forest {
  struct Parser* parser;     // the parser used to create this parse forest
  int prepared;              // is this forest prepared, or was it cleaned up?
  unsigned position;         // sequential position value

  struct Node* root;         // root node of the forest
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

  struct ERed* er_table;     // empty reductions table (right-hand side empty)
  unsigned er_cap;           //   capacity of table
  unsigned er_pos;           //   "current" element
} Forest;

// Create a forest for a given parser.
Forest* forest_create(struct Parser* parser);

// Destroy a forest created with forest_create().
void forest_destroy(Forest* forest);

// Parse some text, populating the parse forest, including its root node.
// Return 0 if all went well, or the number of errors found.
unsigned forest_parse(Forest* forest, Slice text);

// Print a forest in a human-readable format.
void forest_show(Forest* forest);

// Print the forest stack in a human-readable format.
void forest_show_stack(Forest* forest);

#pragma once

#include "symbol.h"
#include "parser.h"

typedef struct Forest {
  Parser* parser;
  int prepared;
  unsigned Position;

  struct Node* NodeTab;
  unsigned NodeE;
  unsigned NodeP;

  struct Vertex* VertTab;
  unsigned VertE;
  unsigned VertP;

  struct Path* PathTab;
  unsigned PathE;

  struct RRed* REDS;
  unsigned RP;
  unsigned RE;

  struct ERed* EREDS;
  unsigned EP;
  unsigned EE;
} Forest;

Forest* forest_create(Parser* parser);
void forest_destroy(Forest* forest);

struct Node* forest_parse(Forest* forest, Slice text);

void forest_show(Forest* forest);
void forest_show_node(Forest* forest, struct Node* node);
void forest_show_stack(Forest* forest);

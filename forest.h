#pragma once

#include "symbol.h"
#include "parser.h"
#include "grammar.h"

typedef struct Forest {
  Parser* parser;
  Grammar* grammar;
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

Forest* forest_create(Grammar* grammar, Parser* parser);
void forest_destroy(Forest* forest);

void forest_prepare(Forest* forest);
void forest_cleanup(Forest* forest);

struct Node* forest_parse(Forest* forest, Slice text);

void forest_show(Forest* forest);
void forest_show_node(Forest* forest, struct Node* node);
void forest_show_stack(Forest* forest);

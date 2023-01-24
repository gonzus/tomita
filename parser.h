#pragma once

#include "grammar.h"

struct Item {
  Symbol* LHS;
  Symbol** RHS;
  Symbol** Pos;
  unsigned Links;
};

struct Items {
  Symbol* Pre;
  unsigned Size;
  struct Item** List;
};

struct Shift {
  Symbol* X;
  int Q;
};

struct Reduce {
  Symbol* LHS;
  Symbol** RHS;
};

struct State {
  unsigned char Final;
  unsigned Es;
  Symbol** EList;
  unsigned Rs;
  struct Reduce* RList;
  unsigned Ss;
  struct Shift* SList;
};

typedef struct Parser {
  Grammar* grammar;
  unsigned Ss;
  struct State* SList;
  struct Items* STab;
} Parser;

Parser* parser_create(Grammar* grammar);
void parser_destroy(Parser* parser);
void parser_show(Parser* parser);

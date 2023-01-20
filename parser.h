#pragma once

#include "grammar.h"

struct Item {
  Symbol* LHS;
  Rule RHS;
  Rule Pos;
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
  Rule RHS;
};

struct State {
  unsigned char Final;
  unsigned Es;
  unsigned Rs;
  unsigned Ss;
  Symbol** EList;
  struct Reduce* RList;
  struct Shift* SList;
};

typedef struct Parser {
  Grammar* grammar;
  struct State* SList;

  struct Items* STab;
  unsigned Ss;
} Parser;

Parser* parser_create(Grammar* grammar);
void parser_destroy(Parser* parser);
void parser_show(Parser* parser);

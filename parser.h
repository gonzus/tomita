#pragma once

#include "grammar.h"

struct Item {
  Symbol* LHS;
  Rule RHS;
  Rule Pos;
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
  struct State* SList;

  struct Items* STab;
  unsigned Ss;
} Parser;

Parser* parser_create(void);
void parser_destroy(Parser* parser);
void parser_build(Parser* parser, Grammar* grammar);
void parser_show(Parser* parser);

#pragma once

#include "slice.h"

typedef struct Symbol {
  Slice Name;
  unsigned char Defined;
  unsigned char Literal;
  unsigned Index;
  unsigned Rules;
  struct Symbol*** RList; // WTF?
  struct Symbol* Next;
  struct Symbol* Tail;
} Symbol;

#if 0
struct Reduce {
  Symbol* LHS;
  Symbol** RHS;
};

struct Shift {
  Symbol* X;
  int Q;
};

// TODO: find a better place for State (and Shift, Reduce)
struct State {
  unsigned char Final;
  unsigned Es;
  unsigned Rs;
  unsigned Ss;
  Symbol** EList;
  struct Reduce* RList;
  struct Shift* SList;
};
#endif

Symbol* symbol_create(Slice name, unsigned literal);
void symbol_destroy(Symbol* symbol);

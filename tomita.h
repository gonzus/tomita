#pragma once

#include "sym.h"

struct Node {
  Symbol Sym;
  unsigned Start;
  unsigned Size;
  unsigned Subs;
  struct Subnode** Sub;
};

extern unsigned Position;
extern struct Node* NodeTab;

struct Node* Parse(void);

void SetTables(void);

void FreeTables(void);

void SHOW_NODE(unsigned N);

void SHOW_FOREST(void);

void SHOW_STACK(void);

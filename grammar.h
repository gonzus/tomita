#pragma once

#include "slice.h"
#include "symtab.h"

typedef struct Grammar {
  SymTab* symtab;
  Symbol* start;
  Symbol* first;
  Symbol* last;
} Grammar;

Grammar* grammar_create(Slice text);
void grammar_destroy(Grammar* grammar);
unsigned grammar_check(Grammar* grammar);

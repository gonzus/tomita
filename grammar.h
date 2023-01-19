#pragma once

#include "slice.h"
#include "sym.h"

typedef struct Grammar {
  Symbol start;
} Grammar;

Grammar* grammar_create(Slice text);
void grammar_destroy(Grammar* grammar);

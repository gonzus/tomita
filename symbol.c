#include <string.h>
#include <stdlib.h>
#include "symbol.h"

static unsigned COUNTER = 0;

Symbol* symbol_create(Slice name, unsigned literal) {
  Symbol* symbol = (Symbol*) malloc(sizeof(Symbol));
  memset(symbol, 0, sizeof(Symbol));
  symbol->Name = name;
  symbol->Literal = literal;
  symbol->Index = COUNTER++;
  return symbol;
}

void symbol_destroy(Symbol* symbol) {
  free(symbol);
}

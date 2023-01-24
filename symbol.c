#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "mem.h"
#include "symbol.h"

static unsigned COUNTER = 0;

Symbol* symbol_create(Slice name, unsigned literal) {
  Symbol* symbol = 0;
  MALLOC(Symbol, symbol);
  symbol->name = name;
  symbol->literal = literal;
  symbol->index = COUNTER++;
  LOG_DEBUG("created symbol #%d [%.*s]", symbol->index, symbol->name.len, symbol->name.ptr);
  return symbol;
}

void symbol_destroy(Symbol* symbol) {
  for (unsigned j = 0; j < symbol->rule_count; ++j) {
    FREE(symbol->rules[j]);
  }
  FREE(symbol->rules);
  FREE(symbol);
}

void symbol_insert_rule(Symbol* symbol, Symbol** SymBuf, Symbol** SymP) {
  LOG_DEBUG("adding rule #%u for symbol %p [%.*s]", symbol->rule_count, (void*) symbol, symbol->name.len, symbol->name.ptr);
  unsigned int k = 0;
  for (k = 0; k < symbol->rule_count; ++k) {
    unsigned diff = 0;
    Symbol** A = 0;
    Symbol** B = 0;
    for (diff = 0, A = SymBuf, B = symbol->rules[k]; *A && *B; ++A, ++B) {
      diff = (*A)->index - (*B)->index;
      if (diff != 0) break;
    }
    if (diff == 0 && *B == 0) {
      if (*A == 0) return;
      break;
    } else if (diff > 0) {
      break;
    }
  }
  if ((symbol->rule_count & 7) == 0)
    REALLOC(Symbol**, symbol->rules, symbol->rule_count + 8);
  for (unsigned j = symbol->rule_count++; j > k; --j) {
    symbol->rules[j] = symbol->rules[j - 1];
  }

  Symbol** rule = 0;
  MALLOC_N(Symbol*, rule, SymP - SymBuf);
  symbol->rules[k] = rule;
  for (k = 0; k < (SymP - SymBuf); ++k) {
    rule[k] = SymBuf[k];
    if (!rule[k]) continue;
    LOG_DEBUG("  symbol #%u -> %p [%.*s]", k, (void*) rule[k], rule[k]->name.len, rule[k]->name.ptr);
  }
}

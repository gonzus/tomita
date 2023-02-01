#include "log.h"
#include "mem.h"
#include "tomita.h"
#include "symbol.h"

Symbol* symbol_create(Slice name, unsigned literal, unsigned* counter) {
  Symbol* symbol = 0;
  MALLOC(Symbol, symbol);
  symbol->name = name;
  symbol->literal = literal;
  symbol->index = (*counter)++;
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
    for (A = SymBuf, B = symbol->rules[k]; !diff && *A && *B; ++A, ++B) {
      diff = (*A)->index - (*B)->index;
    }
    if (diff < 0) continue;
    if (diff > 0) break;
    if (*B == 0) {
      if (*A == 0) return; else break;
    }
  }

  // need more room and to shift later rules
  TABLE_CHECK_GROW(symbol->rules, symbol->rule_count, 8, Symbol**);
  LOG_DEBUG("Growing, current %u, k=%u", symbol->rule_count, k);
  for (unsigned j = symbol->rule_count++; j > k; --j) {
    symbol->rules[j] = symbol->rules[j - 1];
  }

  unsigned size = SymP - SymBuf;
  Symbol** rule = 0;
  MALLOC_N(Symbol*, rule, size);
  symbol->rules[k] = rule;
  for (k = 0; k < size; ++k) {
    rule[k] = SymBuf[k];
    if (!rule[k]) continue;
    LOG_DEBUG("  symbol #%u -> %p [%.*s]", k, (void*) rule[k], rule[k]->name.len, rule[k]->name.ptr);
  }
}

void symbol_print_definition(Symbol* symbol, FILE* fp) {
  fprintf(fp, "%c %u [%.*s] %u %u\n", FORMAT_SYMBOL, symbol->index, symbol->name.len, symbol->name.ptr, (unsigned) symbol->literal, (unsigned) symbol->defined);
}

void symbol_print_rules(Symbol* symbol, FILE* fp) {
  for (unsigned r = 0; r < symbol->rule_count; ++r) {
    fprintf(fp, "%c %u", FORMAT_RULE, symbol->index);
    for (Symbol** rule = symbol->rules[r]; *rule; ++rule) {
      fprintf(fp, " %d", (*rule)->index);
    }
    fprintf(fp, "  # %u\n", r);
  }
}

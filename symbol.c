#include <stdio.h>
#include <limits.h>
#include "log.h"
#include "mem.h"
#include "buffer.h"
#include "tomita.h"
#include "symbol.h"

Symbol* symbol_create(Slice name, unsigned literal, unsigned* counter) {
  Symbol* symbol = 0;
  MALLOC(Symbol, symbol);
  symbol->name = name;
  symbol->literal = literal;
  symbol->index = (*counter)++;
  LOG_DEBUG("created symbol %p index %u [%.*s] %u", symbol, symbol->index, symbol->name.len, symbol->name.ptr, symbol->rs_cap);
  return symbol;
}

void symbol_destroy(Symbol* symbol) {
  for (unsigned j = 0; j < symbol->rs_cap; ++j) {
    FREE(symbol->rs_table[j].rules);
  }
  FREE(symbol->rs_table);
  FREE(symbol);
}

void symbol_show(Symbol* symbol, Symbol* first, Symbol* last) {
  char c = ' ';
  if (symbol == first) {
    c = 'F';
  }
  if (symbol == last) {
    c = 'L';
  }
  printf("literal %1u, defined %1u, index %4u, %c [%.*s] ->", symbol->literal, symbol->defined, symbol->index, c, symbol->name.len, symbol->name.ptr);
  // This is rather obscure... so be it.
  for (unsigned j = 0; j < symbol->rs_cap; ++j) {
    RuleSet* rs = &symbol->rs_table[j];
    printf(" #%u:", rs->index);
    for (Symbol** rules = rs->rules; *rules; ++rules) {
      Symbol* rhs = *rules;
      printf(" %u", rhs->index);
    }
    printf(";");
  }
  printf("\n");
}

void symbol_insert_rule(Symbol* symbol, Symbol** SymBuf, Symbol** SymP, unsigned* counter, unsigned index) {
  if (counter) {
    index = (*counter)++;
  }
  unsigned size = SymP - SymBuf;
  LOG_DEBUG("adding ruleset #%u index %u with %u rhs symbols for symbol %p [%.*s]", symbol->rs_cap, index, size, (void*) symbol, symbol->name.len, symbol->name.ptr);
  unsigned int k = 0;
  for (k = 0; k < symbol->rs_cap; ++k) {
    int diff = 0;
    Symbol** A = 0;
    Symbol** B = 0;
    for (A = SymBuf, B = symbol->rs_table[k].rules; *A && *B; ++A, ++B) {
      diff = (*A)->index - (*B)->index;
      if (diff) break;
    }
    if (diff < 0) continue;
    if (diff > 0) break;
    if (*B == 0) {
      if (*A == 0) return;
      break;
    }
  }

  // need more room and to shift later rulesets
  TABLE_CHECK_GROW(symbol->rs_table, symbol->rs_cap, 8, RuleSet);
  LOG_DEBUG("Growing rulesets, current %u, k=%u", symbol->rs_cap, k);
  for (unsigned j = symbol->rs_cap++; j > k; --j) {
    symbol->rs_table[j] = symbol->rs_table[j - 1];
  }

  symbol->rs_table[k].index = index;
  Symbol** rules = 0;
  MALLOC_N(Symbol*, rules, size);
  symbol->rs_table[k].rules = rules;
  LOG_DEBUG("SYMBOL %p index %u ruleset %u at %p with index %u", symbol, symbol->index, k, rules, index);
  for (k = 0; k < size; ++k) {
    rules[k] = SymBuf[k];
    if (!rules[k]) continue;
    LOG_DEBUG("  symbol #%u -> %p [%.*s]", k, (void*) rules[k], rules[k]->name.len, rules[k]->name.ptr);
  }
}

void symbol_save_definition(Symbol* symbol, Buffer* b) {
  buffer_format_print(b, "%c %u [%.*s] %u %u\n", FORMAT_SYMBOL, symbol->index, symbol->name.len, symbol->name.ptr, (unsigned) symbol->literal, (unsigned) symbol->defined);
}

void symbol_save_rules(Symbol* symbol, Buffer* b) {
  LOG_DEBUG("rulesets for symbol %p index %u [%.*s]", symbol, symbol->index, symbol->name.len, symbol->name.ptr);
  // NOTE:
  //
  // We want to print the rules in a predictable way, to ease comparison using
  // diff. One way of doing this is to sort the rules according to their index;
  // this might be doable but it may also be expensive (to create a temp array
  // just to sort it) or disruptive (if we try to sort in place).
  //
  // What we do is basically an O(N^2) "sort": we loop N times, and each time
  // pick the highest index that we have still not printed.  Given that the
  // expected size of the rulesets seems to be rather small, this could actually
  // end up being *faster* than creating a temp array and using qsort() --
  // still, this needs measurement at some point.
  //
  // Finally, this is symbol_save_rules() -- usage is low anyway.
  unsigned prev = UINT_MAX;
  for (unsigned count = 0; count < symbol->rs_cap; ++count) {
    RuleSet* candidate = 0;
    unsigned largest = 0;
    for (unsigned r = 0; r < symbol->rs_cap; ++r) {
      RuleSet* rs = &symbol->rs_table[r];
      if (rs->index >= prev) continue;
      if (largest > rs->index) continue;
      largest = rs->index;
      candidate = rs;
    }

    LOG_DEBUG("  ruleset %u index %u at %p", count, candidate->index, candidate->rules);
    buffer_format_print(b, "%c %u %u", FORMAT_RULE, candidate->index, symbol->index);
    for (Symbol** rules = candidate->rules; *rules; ++rules) {
      Symbol* rhs = *rules;
      buffer_format_print(b, " %u", rhs->index);
    }
    buffer_format_print(b, "\n");
    prev = largest;
  }
}

RuleSet* symbol_find_ruleset_by_index(Symbol* symbol, unsigned index) {
  // NOTE: this could be made more efficient by storing a NumTab in each
  // Symbol, which would map from the RuleSet index to the RuleSet pointer.
  // However, given that a Symbol usually has at most 10 rule sets, it
  // doesn't seem worthwhile.
  for (unsigned j = 0; j < symbol->rs_cap; ++j) {
    RuleSet* rs = &symbol->rs_table[j];
    if (rs->index == index) return rs;
  }
  return 0;
}

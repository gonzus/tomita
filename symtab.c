#include <stdio.h>
#include "mem.h"
#include "log.h"
#include "numtab.h"
#include "util.h"
#include "buffer.h"
#include "tomita.h"
#include "symbol.h"
#include "symtab.h"

// A work buffer for pointers to symbols, used to store rules.
// Once a rule is completed, we make a call to symbol_insert_rule(), which
// copies all the pointers into rules, and then reset the work buffer.
enum {
  SYMTAB_MAX_SYM = 0x100,
};
static Symbol* sym_buf[SYMTAB_MAX_SYM];
static Symbol** sym_pos;

static unsigned djb2(Slice s);

SymTab* symtab_create(void) {
  SymTab* symtab = 0;
  do {
    MALLOC(SymTab, symtab);
    if (!symtab) break;
    symtab->idx2sym = numtab_create();
  } while (0);
  return symtab;
}

void symtab_destroy(SymTab* symtab) {
  symtab_clear(symtab);
  numtab_destroy(symtab->idx2sym);
  FREE(symtab);
}

void symtab_clear(SymTab* symtab) {
  for (unsigned h = 0; h < SYMTAB_HASH_MAX; ++h) {
    for (Symbol* sym = symtab->table[h]; sym != 0; ) {
      Symbol* tmp = sym;
      sym = sym->nxt_hash;
      symbol_destroy(tmp);
    }
    symtab->table[h] = 0;
  }
  symtab->first = symtab->last = 0;
  symtab->symbol_counter = 0;
  symtab->rules_counter = 0;
  numtab_clear(symtab->idx2sym);
}

void symtab_show(SymTab* symtab) {
  printf("%c%c SYMTAB\n", FORMAT_COMMENT, FORMAT_COMMENT);
  printf("table size: %u, symbol counter: %u, rules counter: %u\n",
         SYMTAB_HASH_MAX, symtab->symbol_counter, symtab->rules_counter);
  for (unsigned h = 0; h < SYMTAB_HASH_MAX; ++h) {
    Symbol* bucket = symtab->table[h];
    if (!bucket) continue;
    printf("== bucket %u ==\n", h);
    for (Symbol* sym = symtab->table[h]; sym != 0; sym = sym->nxt_hash) {
      symbol_show(sym, symtab->first, symtab->last);
    }
  }
}

Symbol* symtab_lookup(SymTab* symtab, Slice name, unsigned char literal, unsigned char insert) {
  Symbol* s = 0;
  unsigned char h = djb2(name);

  // try to locate first
  for (s = symtab->table[h]; s != 0; s = s->nxt_hash) {
    if (s->literal != literal) continue;        // different literal
    if (!slice_equal(s->name, name)) continue;  // different name
    return s;
  }

  // not found
  if (!insert) return 0;

  // must create and chain
  s = symbol_create(name, literal, &symtab->symbol_counter);
  Number* num = numtab_lookup(symtab->idx2sym, s->index, 1);
  assert(num);
  num->ptr = s;

  s->nxt_hash = symtab->table[h];
  symtab->table[h] = s;
  if (symtab->first) {
    symtab->last->nxt_list = s;
  } else {
    symtab->first = s;
  }
  symtab->last = s;
  return s;
}

unsigned symtab_load_from_slice(SymTab* symtab, Slice* text) {
  symtab_clear(symtab);
  unsigned used = 0;

  do {
    unsigned total_symbols = 0;
    unsigned total_rules = 0;
    unsigned sym_seq = 0;
    Symbol* prev = 0;

    SliceLookup lookup_lines = {0};
    while (slice_tokenize_by_byte(*text, '\n', &lookup_lines)) {
      Slice line = slice_trim(lookup_lines.result);
      LOG_DEBUG("read line [%.*s]", line.len, line.ptr);
      char lead = line.ptr[0];
      unsigned pos = 0;
      if (lead == FORMAT_COMMENT) {
        continue;
      }

      ++pos;
      if (lead == FORMAT_SYMTAB) {
        pos = next_number(line, pos, &total_symbols);
        pos = next_number(line, pos, &total_rules);
        LOG_DEBUG("loaded symtab: symbols=%u, rules=%u", total_symbols, total_rules);
        continue;
      }
      if (lead == FORMAT_SYMBOL) {
        unsigned index = 0;
        Slice name;
        unsigned literal = 0;
        unsigned defined = 0;
        pos = next_number(line, pos, &index);
        LOG_DEBUG("INDEX=%u, SEQ=%u", index, sym_seq);
        assert(index == sym_seq);
        ++sym_seq;
        pos = next_string(line, pos, &name);
        pos = next_number(line, pos, &literal);
        pos = next_number(line, pos, &defined);
        Symbol* sym = symtab_lookup(symtab, name, literal, 1);
        LOG_DEBUG("loaded symbol: index=%u, name=[%.*s], literal=%u, defined=%u", index, name.len, name.ptr, literal, defined);
        assert(index == sym->index);
        sym->defined = defined;
        if (prev) prev->nxt_list = sym;
        prev = sym;
        continue;
      }
      if (lead == FORMAT_RULE) {
        unsigned lhs_index = 0;
        unsigned rs_index = 0;
        pos = next_number(line, pos, &rs_index);
        pos = next_number(line, pos, &lhs_index);
        LOG_DEBUG("loaded rule: lhs=%u, rs=%u", lhs_index, rs_index);
        Symbol* lhs = symtab_find_symbol_by_index(symtab, lhs_index);
        assert(lhs);
        sym_pos = sym_buf;
        while (1) {
          unsigned rhs_index = 0;
          pos = next_number(line, pos, &rhs_index);
          if (pos == 0) break;
          LOG_DEBUG("   rhs=%u", rhs_index);
          *sym_pos++ = symtab_find_symbol_by_index(symtab, rhs_index);
        }
        *sym_pos++ = 0;
        symbol_insert_rule(lhs, sym_buf, sym_pos, 0, rs_index);
        ++symtab->rules_counter;
        continue;
      }
      // found something else
      LOG_DEBUG("SYMTAB found other [%c]", lead);
      used = line.ptr - text->ptr;
      break;
    }
    if (!used) used = text->len;
    text->ptr += used;
    text->len -= used;
  } while (0);

  return 0;
}

unsigned symtab_save_to_buffer(SymTab* symtab, Buffer* b) {
  unsigned total_symbols = 0;
  unsigned total_rules = 0;
  for (Symbol* symbol = symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    total_symbols += 1;
    total_rules += symbol->rs_cap;
  }

  buffer_format_print(b, "%c symtab: num_symbols num_rules\n", FORMAT_COMMENT);
  buffer_format_print(b, "%c %u %u\n", FORMAT_SYMTAB, total_symbols, total_rules);
  buffer_format_print(b, "%c symbols (%u): index name literal defined\n", FORMAT_COMMENT, total_symbols);
  for (Symbol* symbol = symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    symbol_save_definition(symbol, b);
  }
  buffer_format_print(b, "%c rules (%u): index lhs [rhs...]\n", FORMAT_COMMENT, total_rules);
  for (Symbol* symbol = symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    symbol_save_rules(symbol, b);
  }

  return 0;
}

Symbol* symtab_find_symbol_by_index(SymTab* symtab, unsigned index) {
  Number* num = numtab_lookup(symtab->idx2sym, index, 0);
  assert(num);
  LOG_DEBUG("NUMTAB %u => %p", index, num->ptr);
  return num->ptr;
}

// http://www.cse.yorku.ca/~oz/hash.html
static unsigned djb2(Slice s) {
  unsigned hash = 5381;
  for (unsigned p = 0; p < s.len; ++p) {
    hash = ((hash << 5) + hash) + s.ptr[p]; /* hash * 33 + c */
  }
  return hash;
}

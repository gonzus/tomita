#include "mem.h"
#include "symtab.h"

SymTab* symtab_create(void) {
  SymTab* symtab = 0;
  MALLOC(SymTab, symtab);
  return symtab;
}

void symtab_destroy(SymTab* symtab) {
  for (unsigned h = 0; h < HASH_MAX; ++h) {
    for (Symbol* sym = symtab->table[h]; sym != 0; ) {
      Symbol* tmp = sym;
      sym = sym->nxt_hash;
      symbol_destroy(tmp);
    }
  }
  FREE(symtab);
}

// TODO: (as in every project) use better hash function?
static unsigned char hash(Slice string) {
  int H = 0;
  for (unsigned pos = 0; pos < string.len; ++pos) {
    H = (H << 1) ^ string.ptr[pos];
  }
  return H & (HASH_MAX - 1);
}

int symtab_lookup(SymTab* symtab, Slice name, unsigned char literal, Symbol** sym) {
  Symbol* s = 0;
  unsigned char h = hash(name);

  // try to locate first
  for (s = symtab->table[h]; s != 0; s = s->nxt_hash) {
    if (s->literal != literal) continue;        // different literal
    if (!slice_equal(s->name, name)) continue;  // different name
    *sym = s;
    return 0;
  }

  // not found, must create and chain
  s = symbol_create(name, literal, &symtab->counter);
  s->nxt_hash = symtab->table[h];
  symtab->table[h] = s;
  *sym = s;
  return 1;
}

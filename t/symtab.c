#include <tap.h>
#include "util.h"
#include "symtab.h"

static void test_build_symtab(void) {
  struct Data {
    const char* name;
    unsigned char literal;
  };
  static const char* Type[] = {
    "non-terminal",
    "terminal",
  };
  static struct Data Dwarves[] = {
    { "Thorin", 1 },
    { "Balin" , 0 },
    { "Dwalin", 1 },
    { "Fili"  , 0 },
    { "Kili"  , 1 },
    { "Dori"  , 0 },
    { "Ori"   , 1 },
    { "Nori"  , 0 },
    { "Oin"   , 1 },
    { "Gloin" , 0 },
    { "Bifur" , 1 },
    { "Bofur" , 0 },
    { "Bombur", 1 },
  };
  static struct Data Elves[] = {
    { "Feanor"   , 1 },
    { "Finarfin" , 0 },
    { "Fingolfin", 1 },
    { "Finrod"   , 0 },
    { "Galadriel", 1 },
    { "Elrond"   , 0 },
    { "Legolas"  , 1 },
  };

  unsigned errors = 0;
  SymTab* symtab = 0;
  Buffer created; buffer_build(&created);
  Buffer loaded; buffer_build(&loaded);
  do {
    ok(1, "=== TESTING symtab ===");

    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    for (unsigned j = 0; j < ALEN(Dwarves); ++j) {
      unsigned pos = j;
      struct Data* data = &Dwarves[pos];
      Slice name = slice_from_string(data->name, 0);
      Symbol* s = symtab_lookup(symtab, name, data->literal, 1);
      ok(s->index == pos, "inserted symbol [%.*s] with index %u", name.len, name.ptr, pos);
    }

    for (unsigned j = 0; j < ALEN(Dwarves); ++j) {
      unsigned pos = ALEN(Dwarves) - j - 1;
      struct Data* data = &Dwarves[pos];
      Slice name = slice_from_string(data->name, 0);
      for (unsigned wanted = 0; wanted < 2; ++wanted) {
        unsigned must = wanted == data->literal;
        Symbol* s = symtab_lookup(symtab, name, wanted, 0);
        ok(must ? (s && s->index == pos) : (!s),
           "symbol [%.*s] %s as %s with index %u",
           name.len, name.ptr, must ? "found" : "not found", Type[wanted], pos);
      }
    }

    for (unsigned j = 0; j < ALEN(Elves); ++j) {
      unsigned pos = j;
      struct Data* data = &Elves[pos];
      Slice name = slice_from_string(data->name, 0);
      for (unsigned wanted = 0; wanted < 2; ++wanted) {
        Symbol* s = symtab_lookup(symtab, name, wanted, 0);
        ok(s == 0, "did not find symbol [%.*s] as %s", name.len, name.ptr, Type[wanted]);
      }
    }

    buffer_clear(&created);
    errors = symtab_save_to_buffer(symtab, &created);
    ok(errors == 0, "can save a created symtab to a buffer");
    Slice x = buffer_slice(&created);
    Slice c = x;
    ok(c.len > 0, "saved created symtab has a valid non-zero size of %u bytes", c.len);

    errors = symtab_load_from_slice(symtab, &x);
    ok(errors == 0, "can load a created symtab from a buffer");
    ok(x.len == 0, "after loading, there was nothing after the symtab contents");

    buffer_clear(&loaded);
    errors = symtab_save_to_buffer(symtab, &loaded);
    Slice l = buffer_slice(&loaded);
    ok(errors == 0, "can save a loaded symtab to a buffer");
    ok(l.len > 0, "saved loaded symtab has a valid non-zero size of %u bytes", l.len);

    ok(slice_equal(c, l), "created and loaded grammars are identical");
  } while (0);
  buffer_destroy(&loaded);
  buffer_destroy(&created);
  if (symtab) symtab_destroy(symtab);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_build_symtab();
  } while (0);

  done_testing();
}

#include <tap.h>
#include "util.h"
#include "symtab.h"

#define ALEN(a) (unsigned) (sizeof(a) / sizeof(a[0]))

static void test_build_symtab(void) {
  static struct Data {
    const char* name;
    unsigned char literal;
  } Data[] = {
    { "thorin", 1 },
    { "balin" , 0 },
    { "dwalin", 1 },
    { "fili"  , 0 },
    { "kili"  , 1 },
    { "dori"  , 0 },
    { "ori"   , 1 },
    { "nori"  , 0 },
    { "oin"   , 1 },
    { "gloin" , 0 },
    { "bifur" , 1 },
    { "bofur" , 0 },
    { "bombur", 1 },
  };

  unsigned errors = 0;
  SymTab* symtab = 0;
  Buffer created; buffer_build(&created);
  Buffer loaded; buffer_build(&loaded);
  do {
    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    for (unsigned j = 0; j < ALEN(Data); ++j) {
      unsigned pos = j;
      struct Data* data = &Data[pos];
      Slice name = slice_from_string(data->name, 0);
      Symbol* s = symtab_lookup(symtab, name, data->literal);
      ok(s->index == pos, "inserted symbol [%.*s] with index %u", s->name.len, s->name.ptr, s->index);
    }

    for (unsigned j = 0; j < ALEN(Data); ++j) {
      unsigned pos = ALEN(Data) - j - 1;
      struct Data* data = &Data[pos];
      Slice name = slice_from_string(data->name, 0);
      Symbol* s = symtab_lookup(symtab, name, data->literal);
      ok(s->index == pos, "found symbol [%.*s] with index %u", s->name.len, s->name.ptr, s->index);
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

#include <tap.h>
#include "buffer.h"
#include "util.h"
#include "symtab.h"
#include "grammar.h"


static void test_build_grammar(void) {
  typedef struct Data {
    const char* file;
    const char* name;
  } Data;
  static Data grammars[] = {
    { "t/fixtures/yacc.grammar", "traditional yacc" },
    { "t/fixtures/peg.grammar", "PEG style" },
  };

  unsigned errors = 0;
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Buffer compiled; buffer_build(&compiled);
  Buffer loaded; buffer_build(&loaded);
  Buffer grammar_src; buffer_build(&grammar_src);
  do {
    ok(1, "=== TESTING grammar ===");

    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    grammar = grammar_create(symtab);
    ok(grammar != 0, "can create a grammar");
    if (!grammar) break;

    for (unsigned j = 0; j < ALEN(grammars); ++j) {
      const char* name = grammars[j].name;

      buffer_clear(&grammar_src);
      unsigned bytes = file_slurp(grammars[j].file, &grammar_src);
      ok(bytes > 0, "grammar source for '%s' can be read, it has %u bytes", name, bytes);
      Slice source = buffer_slice(&grammar_src);

      errors = grammar_compile_from_slice(grammar, source);
      ok(errors == 0, "can compile grammar '%s' from source", name);

#if 0
      grammar_show(grammar);
#endif

      buffer_clear(&compiled);
      errors = grammar_save_to_buffer(grammar, &compiled);
      ok(errors == 0, "can save compiled grammar '%s' to a buffer", name);
      Slice c = buffer_slice(&compiled);
      ok(compiled.len > 0, "saved compiled grammar '%s' has a valid non-zero size of %u bytes", name, compiled.len);

      errors = grammar_load_from_slice(grammar, c);
      ok(errors == 0, "can load grammar '%s' from a buffer", name);

      buffer_clear(&loaded);
      errors = grammar_save_to_buffer(grammar, &loaded);
      ok(errors == 0, "can save loaded grammar '%s' to a buffer", name);
      Slice l = buffer_slice(&loaded);
      ok(loaded.len > 0, "saved loaded grammar '%s' has a valid non-zero size of %u bytes", name, loaded.len);

      ok(slice_equal(c, l), "compiled and loaded grammars '%s' are identical", name);
      grammar_clear(grammar);
      symtab_clear(symtab);
      // printf(">>>\n%.*s<<<\n", c.len, c.ptr);
    }
  } while (0);
  buffer_destroy(&grammar_src);
  buffer_destroy(&loaded);
  buffer_destroy(&compiled);
  if (grammar) grammar_destroy(grammar);
  if (symtab) symtab_destroy(symtab);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_build_grammar();
  } while (0);

  done_testing();
}

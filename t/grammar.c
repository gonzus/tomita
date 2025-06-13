#include <tap.h>
#include "util.h"
#include "symtab.h"
#include "grammar.h"

static void test_build_grammar(void) {
  typedef struct Data {
    const char* name;
    const char* rules;
  } Data;
  static Data grammars[] = {
    {
      "traditional yacc",
      "Expr : Expr '-' Expr"
      "     | Expr '*' Expr"
      "     | digit"
      "     ;"
      ""
      "'-';"
      "'*';"
      "digit = '0' '1' '2' '3' '4' '5' '6' '7' '8' '9';"
    },
    {
      "PEG style",
      "Expr <- Expr '-' Expr"
      "     /  Expr '*' Expr"
      "     /  digit"
      "     ;"
      ""
      "'-';"
      "'*';"
      "digit = '0' '1' '2' '3' '4' '5' '6' '7' '8' '9';"
    },
  };

  unsigned errors = 0;
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Buffer compiled; buffer_build(&compiled);
  Buffer loaded; buffer_build(&loaded);
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
      Slice source = slice_from_string(grammars[j].rules, 0);
      errors = grammar_compile_from_slice(grammar, source);
      ok(errors == 0, "can compile grammar '%s' from source", name);

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

#include <tap.h>
#include "util.h"
#include "grammar.h"

static void test_build_grammar(void) {
  static const char* grammar_source =
    "Expr : Expr '-' Expr"
    "     | Expr '*' Expr"
    "     | digit"
    "     ;"
    ""
    "'-';"
    "'*';"
    "digit = '0' '1' '2' '3' '4' '5' '6' '7' '8' '9';"
  ;

  unsigned errors = 0;
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Buffer compiled; buffer_build(&compiled);
  Buffer loaded; buffer_build(&loaded);
  do {
    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    grammar = grammar_create(symtab);
    ok(grammar != 0, "can create a grammar");
    if (!grammar) break;

    Slice source = slice_from_string(grammar_source, 0);
    errors = grammar_compile_from_slice(grammar, source);
    ok(errors == 0, "can compile a grammar from source");

    buffer_clear(&compiled);
    errors = grammar_save_to_buffer(grammar, &compiled);
    ok(errors == 0, "can save a compiled grammar to a buffer");
    Slice c = buffer_slice(&compiled);
    ok(compiled.len > 0, "saved compiled grammar has a valid non-zero size of %u bytes", compiled.len);

    errors = grammar_load_from_slice(grammar, c);
    ok(errors == 0, "can load a grammar from a buffer");

    buffer_clear(&loaded);
    errors = grammar_save_to_buffer(grammar, &loaded);
    ok(errors == 0, "can save a loaded grammar to a buffer");
    Slice l = buffer_slice(&loaded);
    ok(loaded.len > 0, "saved loaded grammar has a valid non-zero size of %u bytes", loaded.len);

    ok(slice_equal(c, l), "compiled and loaded grammars are identical");

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

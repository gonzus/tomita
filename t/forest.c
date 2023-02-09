#include <tap.h>
#include "util.h"
#include "forest.h"

static void test_build_forest(void) {
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
  typedef struct Expr {
    int value;
    unsigned branches;
    const char* what;
  } Expr;
  // TODO: test with invalid expresions
  // For example, "2 + 3" is being recognized, '+' shows up as a digit?!?
  static const Expr exprs[] = {
    {  3, 2, "9 - 3 * 2" },
    { 16, 2, "6 * 3 - 2" },
    { 24, 3, "1 * 2 * 3 * 4" },
  };

  unsigned errors = 0;
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Parser* parser = 0;
  Forest* forest = 0;
  do {
    ok(1, "=== TESTING forest ===");

    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    grammar = grammar_create(symtab);
    ok(grammar != 0, "can create a grammar");
    if (!grammar) break;

    Slice g = slice_from_string(grammar_source, 0);
    errors = grammar_compile_from_slice(grammar, g);
    ok(errors == 0, "can compile a grammar from source");

    parser = parser_create(symtab);
    ok(parser != 0, "can create a parser");
    if (!parser) break;

    errors = parser_build_from_grammar(parser, grammar);
    ok(errors == 0, "can build a parser from a grammar");

    forest = forest_create(parser);
    ok(forest != 0, "can create a forest");
    if (!forest) break;

    for (unsigned j = 0; j < ALEN(exprs); ++j) {
      // int expected = exprs[j].value;
      const char* expr = exprs[j].what;
      Slice e = slice_from_string(expr, 0);
      errors = forest_parse(forest, e);
      ok(errors == 0, "can parse source '%s' into a parse forest", expr);
      ok(!!forest->root, "parsed source '%s' has a root node", expr);
      if (!forest->root) continue;
      ok(forest->root->sub_cap == exprs[j].branches, "root node for '%s' has the expected %d branches", expr, exprs[j].branches);

#if 0
      forest_show(forest);
#endif
    }
  } while (0);
  if (forest) forest_destroy(forest);
  if (parser) parser_destroy(parser);
  if (grammar) grammar_destroy(grammar);
  if (symtab) symtab_destroy(symtab);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_build_forest();
  } while (0);

  done_testing();
}

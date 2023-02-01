#include <tap.h>
#include "util.h"
#include "grammar.h"

static void test_build_grammar(void) {
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  do {
    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    grammar = grammar_create(symtab);
    ok(grammar != 0, "can create a grammar");
  } while (0);
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

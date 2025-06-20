#include <tap.h>
#include "buffer.h"
#include "util.h"
#include "forest.h"
#include "tomita.h"

#define GRAMMAR_EXPR "t/fixtures/expr.grammar"

static void test_tomita_build_and_parse_ok(void) {
  static const char* expr_source =
    "2 - 3 * 4"
  ;
  unsigned errors = 0;
  Tomita* tomita = 0;
  Buffer grammar_src; buffer_build(&grammar_src);
  do {
    ok(1, "=== TESTING tomita good ===");

    unsigned bytes = file_slurp(GRAMMAR_EXPR, &grammar_src);
    ok(bytes > 0, "grammar fixture can be read, it has %u bytes", bytes);

    tomita = tomita_create(0, 0);
    ok(tomita != 0, "can create a Tomita");
    if (!tomita) break;

    Slice g = buffer_slice(&grammar_src);
    errors = tomita_grammar_compile_from_slice(tomita, g);
    ok(errors == 0, "tomita can compile a grammar from source");

    errors = tomita_parser_build_from_grammar(tomita);
    ok(errors == 0, "tomita can build a parser from a grammar");

    Slice e = slice_from_string(expr_source, 0);
    errors = tomita_forest_parse_from_slice(tomita, e);
    ok(errors == 0, "tomita can parse a source into a parse forest");
    ok(tomita->forest->root->sub_cap == 2, "root node has the expected %d branches", 2);

#if 0
    tomita_show(tomita);
#endif
  } while (0);
  buffer_destroy(&grammar_src);
  if (tomita) tomita_destroy(tomita);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_tomita_build_and_parse_ok();
  } while (0);

  done_testing();
}

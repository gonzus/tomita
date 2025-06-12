#include <tap.h>
#include "util.h"
#include "forest.h"
#include "tomita.h"

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
static const char* expr_source =
  "2 - 3 * 4"
;

static void test_tomita_build_and_parse_ok(void) {
  unsigned errors = 0;
  Tomita* tomita = 0;
  do {
    ok(1, "=== TESTING tomita good ===");

    tomita = tomita_create();
    ok(tomita != 0, "can create a Tomita");
    if (!tomita) break;

    Slice g = slice_from_string(grammar_source, 0);
    errors = tomita_grammar_compile_from_slice(tomita, g);
    ok(errors == 0, "tomita can compile a grammar from source");

    errors = tomita_parser_build_from_grammar(tomita);
    ok(errors == 0, "tomita can build a parser from a grammar");

    Slice e = slice_from_string(expr_source, 0);
    errors = tomita_forest_parse_from_slice(tomita, e);
    ok(errors == 0, "tomita can parse a source into a parse forest");
    ok(tomita->forest->root->sub_cap == 2, "root node has the expected %d branches", 2);
  } while (0);
  if (tomita) tomita_destroy(tomita);
}

static void test_tomita_build_without_grammar(void) {
  unsigned errors = 0;
  Tomita* tomita = 0;
  do {
    ok(1, "=== TESTING tomita without grammar ===");

    tomita = tomita_create();
    ok(tomita != 0, "can create a Tomita");
    if (!tomita) break;

    errors = tomita_grammar_show(tomita);
    ok(errors > 0, "tomita cannot show a grammar without a grammar");

    errors = tomita_grammar_write_to_buffer(tomita, 0);
    ok(errors > 0, "tomita cannot write a grammar without a grammar");

    errors = tomita_parser_show(tomita);
    ok(errors > 0, "tomita cannot show a parser without a grammar");

    errors = tomita_parser_build_from_grammar(tomita);
    ok(errors > 0, "tomita cannot build a parser without a grammar");

    errors = tomita_parser_write_to_buffer(tomita, 0);
    ok(errors > 0, "tomita cannot write a parser without a grammar");

    errors = tomita_forest_show(tomita);
    ok(errors > 0, "tomita cannot show a forest without a grammar");

    Slice e = slice_from_string("", 0);
    errors = tomita_forest_parse_from_slice(tomita, e);
    ok(errors > 0, "tomita cannot parse a source into a parse forest without a grammar");
  } while (0);
  if (tomita) tomita_destroy(tomita);
}

static void test_tomita_build_without_parser(void) {
  unsigned errors = 0;
  Tomita* tomita = 0;
  do {
    ok(1, "=== TESTING tomita without paser ===");

    tomita = tomita_create();
    ok(tomita != 0, "can create a Tomita");
    if (!tomita) break;

    Slice g = slice_from_string(grammar_source, 0);
    errors = tomita_grammar_compile_from_slice(tomita, g);
    ok(errors == 0, "tomita can compile a grammar from source");

    Slice e = slice_from_string("", 0);
    errors = tomita_forest_parse_from_slice(tomita, e);
    ok(errors > 0, "tomita cannot parse a source into a parse forest without a grammar");
  } while (0);
  if (tomita) tomita_destroy(tomita);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_tomita_build_and_parse_ok();
    test_tomita_build_without_grammar();
    test_tomita_build_without_parser();
  } while (0);

  done_testing();
}

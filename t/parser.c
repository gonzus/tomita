#include <tap.h>
#include "util.h"
#include "symtab.h"
#include "grammar.h"
#include "parser.h"

#define GRAMMAR_EXPR "t/fixtures/expr.grammar"

static void test_build_parser(void) {

  unsigned errors = 0;
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Parser* parser = 0;
  Buffer compiled; buffer_build(&compiled);
  Buffer loaded; buffer_build(&loaded);
  Buffer grammar_src; buffer_build(&grammar_src);
  do {
    ok(1, "=== TESTING parser ===");

    unsigned bytes = file_slurp(GRAMMAR_EXPR, &grammar_src);
    ok(bytes > 0, "grammar fixture can be read, it has %u bytes", bytes);

    symtab = symtab_create();
    ok(symtab != 0, "can create a symtab");
    if (!symtab) break;

    grammar = grammar_create(symtab);
    ok(grammar != 0, "can create a grammar");
    if (!grammar) break;

    parser = parser_create(symtab);
    ok(parser != 0, "can create a parser");
    if (!parser) break;

    Slice source = buffer_slice(&grammar_src);
    errors = grammar_compile_from_slice(grammar, source);
    ok(errors == 0, "can compile a grammar from source");

    errors = parser_build_from_grammar(parser, grammar);
    ok(errors == 0, "can build a parser from a grammar");

#if 0
    parser_show(parser);
#endif

    buffer_clear(&compiled);
    errors = parser_save_to_buffer(parser, &compiled);
    ok(errors == 0, "can save a compiled parser to a buffer");
    Slice c = buffer_slice(&compiled);
    ok(compiled.len > 0, "saved compiled parser has a valid non-zero size of %u bytes", compiled.len);

    errors = parser_load_from_slice(parser, c);
    ok(errors == 0, "can load a parser from a buffer");

    buffer_clear(&loaded);
    errors = parser_save_to_buffer(parser, &loaded);
    ok(errors == 0, "can save a loaded parser to a buffer");
    Slice l = buffer_slice(&loaded);
    ok(loaded.len > 0, "saved loaded parser has a valid non-zero size of %u bytes", loaded.len);

    ok(slice_equal(c, l), "compiled and loaded parsers are identical");
    // printf(">>>\n%.*s<<<\n", c.len, c.ptr);

  } while (0);
  buffer_destroy(&loaded);
  buffer_destroy(&compiled);
  buffer_destroy(&grammar_src);
  if (parser) parser_destroy(parser);
  if (grammar) grammar_destroy(grammar);
  if (symtab) symtab_destroy(symtab);
}

int main (int argc, char* argv[]) {
  UNUSED(argc);
  UNUSED(argv);

  do {
    test_build_parser();
  } while (0);

  done_testing();
}

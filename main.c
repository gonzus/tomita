#include <stdio.h>
#include <unistd.h>
#include "util.h"
#include "log.h"
#include "timer.h"
#include "forest.h"

#define MAX_BUF (1024*1024)

static char* opt_grammar_file = 0;
static int opt_grammar = 0;
static int opt_stack = 0;
static int opt_table = 0;

static void process_line(Forest* forest, Slice line) {
  LOG_INFO("parsing line [%.*s]", line.len, line.ptr);

  struct Node* Nd = forest_parse(forest, line);
  LOG_INFO("parsed input, got %p", Nd);
  if (Nd) {
    putchar('*');
    forest_show_node(forest, Nd);
    putchar('.');
    putchar('\n');
  }

  LOG_INFO("complete forest:");
  forest_show(forest);

  if (opt_stack) {
    LOG_INFO("parse stack:");
    forest_show_stack(forest);
  }
}

static void process_path(Forest* forest, const char* path) {
  FILE* fp = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) break;

    while (1) {
      char buf[MAX_BUF];
      if (!fgets(buf, MAX_BUF, fp)) break;
      Slice raw = slice_from_string(buf, 0);
      Slice line = slice_trim(raw);
      process_line(forest, line);
    }
  } while (0);
  if (fp)
    fclose(fp);
}

static void show_usage(const char* prog) {
  printf(
      "Usage: %s -f file [-gts] file ...\n"
      "   -f  use this grammar file (required)\n"
      "   -g  display input grammar\n"
      "   -t  display parsing table\n"
      "   -s  display parsing stack\n"
      "   -?  print this help\n",
      prog
    );
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "gtsf:?")) != -1) {
    switch (c) {
      case 'g':
        opt_grammar = 1;
        break;
      case 't':
        opt_table = 1;
        break;
      case 's':
        opt_stack = 1;
        break;
      case 'f':
        opt_grammar_file = optarg;
        break;
      case '?':
      default:
        show_usage(argv[0]);
        return 0;
    }
  }
  if (!opt_grammar_file) {
    show_usage(argv[0]);
    return 0;
  }
  argc -= optind;
  argv += optind;

  Buffer data; buffer_build(&data);
  SymTab* symtab = 0;
  Grammar* grammar = 0;
  Parser* parser = 0;
  Forest* forest = 0;
  do {
    unsigned errors = 0;
    Timer timer;

    timer_start(&timer);
    symtab = symtab_create();
    timer_stop(&timer);
    if (!symtab) {
      LOG_INFO("could not create symtab");
      break;
    };
    LOG_INFO("symtab created in %luus", timer_elapsed_us(&timer));

    timer_start(&timer);
    grammar = grammar_create(symtab);
    timer_stop(&timer);
    if (!grammar) {
      LOG_INFO("could not create grammar");
      break;
    };
    LOG_INFO("grammar created in %luus", timer_elapsed_us(&timer));

    buffer_clear(&data);
    timer_start(&timer);
    slurp_file(opt_grammar_file, &data);
    timer_stop(&timer);
    if (data.len == 0) {
      LOG_INFO("could not read grammar file [%s]", opt_grammar_file);
      break;
    };
    LOG_INFO("read %u bytes from grammar file [%s] in %luus", data.len, opt_grammar_file, timer_elapsed_us(&timer));
    LOG_INFO("\n%.*s", data.len, data.ptr);

    Slice raw = buffer_slice(&data);
    Slice text_grammar = slice_trim(raw);
    timer_start(&timer);
    errors = grammar_build_from_text(grammar, text_grammar);
    timer_stop(&timer);
    if (errors) {
      LOG_INFO("grammar has %u errors", errors);
      break;
    }
    LOG_INFO("built grammar from source in %luus", timer_elapsed_us(&timer));

    if (opt_grammar) {
      grammar_show(grammar);

      FILE* fp = fopen("/tmp/tomita/grammar.out", "w+");
      if (!fp) {
        LOG_INFO("could not create compiled grammar file");
        break;
      }

      grammar_save_to_stream(grammar, fp);
      rewind(fp);
      buffer_clear(&data);
      slurp_stream(fp, &data);
      fclose(fp);

      Slice compiled_grammar = buffer_slice(&data);
      timer_start(&timer);
      errors = grammar_load_from_slice(grammar, compiled_grammar);
      timer_stop(&timer);
      if (errors) {
        LOG_INFO("compiled grammar has %u errors", errors);
        break;
      }
      LOG_INFO("loaded compiled grammar from file in %luus", timer_elapsed_us(&timer));
      grammar_show(grammar);
    }

    timer_start(&timer);
    parser = parser_create(symtab);
    timer_stop(&timer);
    if (!parser) {
      LOG_INFO("could not create parser");
      break;
    };
    LOG_INFO("parser created in %luus", timer_elapsed_us(&timer));

    timer_start(&timer);
    errors = parser_build_from_grammar(parser, grammar);
    timer_stop(&timer);
    if (errors) {
      LOG_INFO("parser has %u errors", errors);
      break;
    }
    LOG_INFO("built parser from grammar in %luus", timer_elapsed_us(&timer));

    if (opt_table) {
      parser_show(parser);

      FILE* fp = fopen("/tmp/tomita/parser.out", "w+");
      if (!fp) {
        LOG_INFO("could not create compiled parser file");
        break;
      }

      parser_save_to_stream(parser, fp);
      rewind(fp);
      buffer_clear(&data);
      slurp_stream(fp, &data);
      fclose(fp);

#if 1
      Slice compiled_parser = buffer_slice(&data);
      timer_start(&timer);
      errors = parser_load_from_slice(parser, compiled_parser);
      timer_stop(&timer);
      if (errors) {
        LOG_INFO("compiled parser has %u errors", errors);
        break;
      }
      LOG_INFO("loaded compiled parser from file in %luus", timer_elapsed_us(&timer));
      parser_show(parser);
#endif
    }

#if 1
    forest = forest_create(parser);
    LOG_INFO("created forest");

    for (int j = 0; j < argc; ++j) {
      process_path(forest, argv[j]);
    }
#endif
  } while (0);

  if (forest)
    forest_destroy(forest);
  if (parser)
    parser_destroy(parser);
  if (grammar)
    grammar_destroy(grammar);
  if (symtab)
    symtab_destroy(symtab);
  buffer_destroy(&data);

  return 0;
}

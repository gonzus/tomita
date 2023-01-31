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

  Buffer src; buffer_build(&src);
  Timer timer;
  Grammar* grammar = 0;
  Parser* parser = 0;
  Forest* forest = 0;
  do {
#if 0
    unsigned len = slurp_file(opt_grammar_file, &src);
    if (len <= 0) break;
    LOG_INFO("read %u bytes from [%s]", len, opt_grammar_file);
    Slice raw = buffer_slice(&src);
    Slice text_grammar = slice_trim(raw);
    if (opt_grammar) {
      printf("%.*s\n", text_grammar.len, text_grammar.ptr);
    }
#endif

    grammar = grammar_create();
    if (!grammar) break;

    timer_start(&timer);
    slurp_file(opt_grammar_file, &src);
    Slice raw = buffer_slice(&src);
    LOG_INFO("read %u bytes from [%s]", raw.len, opt_grammar_file);
    Slice text_grammar = slice_trim(raw);
    unsigned errors = grammar_build_from_text(grammar, text_grammar);
    timer_stop(&timer);
    if (errors) {
      LOG_INFO("grammar has %u errors", errors);
      break;
    }
    LOG_INFO("built grammar from source in %luus", timer_elapsed_us(&timer));
    if (opt_grammar) {
      grammar_show(grammar);

      FILE* fp = fopen("/tmp/tomita/grammar.out", "w+");
      grammar_save_to_stream(grammar, fp);
      rewind(fp);

      timer_start(&timer);
      grammar_load_from_stream(grammar, fp);
      timer_stop(&timer);
      fclose(fp);

      LOG_INFO("loaded grammar from file in %luus", timer_elapsed_us(&timer));
    }

    parser = parser_create(grammar);
    if (!parser) break;
    LOG_INFO("created parser");
    if (opt_table) {
      parser_show(parser);

      FILE* fp = fopen("/tmp/tomita/parser.out", "w+");
      parser_save_to_stream(parser, fp);
      fclose(fp);
    }

    forest = forest_create(parser);
    LOG_INFO("created forest");

    for (int j = 0; j < argc; ++j) {
      process_path(forest, argv[j]);
    }
  } while (0);

  if (forest)
    forest_destroy(forest);
  if (parser)
    parser_destroy(parser);
  if (grammar)
    grammar_destroy(grammar);
  buffer_destroy(&src);

  return 0;
}

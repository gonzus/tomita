#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "util.h"
#include "log.h"
#include "forest.h"

#define MAX_BUF (1024*1024)

static char* opt_grammar = 0;
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
      "Usage: %s -gsc? data ...\n"
      "    -g ...... use this grammar file (required)\n"
      "    -c ...... display parsing table\n"
      "    -s ...... display parsing stack\n"
      "    -? ...... print this help\n",
      prog
    );
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "csg:?")) != -1) {
    switch (c) {
      case 'g':
        opt_grammar = optarg;
        break;
      case 'c':
        opt_table = 1;
        break;
      case 's':
        opt_stack = 1;
        break;
      case '?':
      default:
        show_usage(argv[0]);
        return 0;
    }
  }
  if (!opt_grammar) {
    show_usage(argv[0]);
    return 0;
  }
  argc -= optind;
  argv += optind;

  Grammar* grammar = 0;
  Parser* parser = 0;
  Forest* forest = 0;
  do {
    char buf[MAX_BUF];
    unsigned len = slurp_file(opt_grammar, buf, MAX_BUF);
    if (len <= 0) break;
    LOG_INFO("read %u bytes from [%s]", len, opt_grammar);
    Slice text_grammar = slice_from_memory(buf, len);

    grammar = grammar_create(text_grammar);
    if (!grammar) break;
    LOG_INFO("created grammar");

    unsigned errors = grammar_check(grammar);
    if (errors) {
      LOG_INFO("grammar has %u errors", errors);
      break;
    }

    parser = parser_create(grammar);
    if (!parser) break;
    LOG_INFO("created parser");
    if (opt_table) {
      parser_show(parser);
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

  return 0;
}

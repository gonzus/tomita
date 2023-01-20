#include <stdio.h>
#include "util.h"
#include "log.h"
#include "forest.h"

#define MAX_BUF (1024*1024)

static void process_line(Forest* forest, Slice line, int DoS) {
    LOG_INFO("parsing line [%.*s]", line.len, line.ptr);

    struct Node* Nd = forest_parse(forest, line);
    LOG_INFO("parsed input, got %p", Nd);
    if (Nd) {
      putchar('*');
      forest_show_node(forest, Nd);
      printf(".\n");
    }

    LOG_INFO("complete forest:");
    forest_show(forest);

    if (DoS) {
      LOG_INFO("parse stack:");
      forest_show_stack(forest);
    }
}

static void process_path(Forest* forest, const char* path, int DoS) {
  FILE* fp = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) break;

    while (1) {
      char buf[MAX_BUF];
      if (!fgets(buf, MAX_BUF, fp)) break;
      Slice raw = slice_from_string(buf, 0);
      Slice line = slice_trim(raw);
      process_line(forest, line, DoS);
    }
  } while (0);
  if (fp)
    fclose(fp);
}

int main(int argc, char **argv) {
  int Arg; char *AP;
  int DoC = 0, DoS = 0, DoH = 0;
  for (Arg = 1; Arg < argc; Arg++) {
    AP = argv[Arg];
    if (*AP++ != '-') break;
    for (; *AP != '\0'; AP++)
      switch (*AP) {
        case 's': DoS++; break;
        case 'c': DoC++; break;
        default: DoH++; break;
      }
  }
  if (DoH || Arg >= argc) {
    printf(
        "Usage: tom -sc? grammar\n"
        "    -c ...... display parsing table\n"
        "    -s ...... display parsing stack\n"
        "    -h/-? ... print this list\n"
      );
    return 1;
  }

  Grammar* grammar = 0;
  Parser* parser = 0;
  Forest* forest = 0;

  do {
    char buf[MAX_BUF];
    unsigned len = slurp_file(argv[Arg+0], buf, MAX_BUF);
    if (len <= 0) break;
    LOG_INFO("read %u bytes from [%s]", len, argv[Arg+0]);
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
    if (DoC) {
      parser_show(parser);
    }

    forest = forest_create(parser);
    LOG_INFO("created forest");

    process_path(forest, argv[Arg+1], DoS);
  } while (0);

  if (forest)
    forest_destroy(forest);
  if (parser)
    parser_destroy(parser);
  if (grammar)
    grammar_destroy(grammar);

  return 0;
}

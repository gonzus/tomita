#include <stdio.h>
#include "util.h"
#include "log.h"
// #include "lex.h"
// #include "sym.h"
// #include "tomita.h"
#include "grammar.h"
#include "parser.h"

#define MAX_BUF (1024*1024)

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

#if 1
  Grammar* grammar = 0;
  Parser* parser = 0;
  do {
    char buf[MAX_BUF];
    unsigned len = slurp_file(argv[Arg], buf, MAX_BUF);
    if (len <= 0) break;
    LOG_INFO("read %u bytes from [%s]", len, argv[Arg]);
    Slice text = slice_from_memory(buf, len);

    grammar = grammar_create(text);
    if (!grammar) break;
    LOG_INFO("created grammar");

    unsigned errors = grammar_check(grammar);
    if (errors) {
      LOG_INFO("grammar has %u errors", errors);
      break;
    }

    parser = parser_create();
    if (!parser) break;
    LOG_INFO("created parser");

    parser_build(parser, grammar);
    LOG_INFO("built parser from grammar");
    parser_show(parser);
  } while (0);

  if (parser)
    parser_destroy(parser);
  if (grammar)
    grammar_destroy(grammar);
#endif

#if 0
  if (!OPEN(argv[Arg])) {
    fprintf(stderr, "Cannot open %s.\n", argv[Arg]);
    return 1;
  }

  int ERRORS = 0;
  Symbol Start;
  CreateGrammar(&Start, &ERRORS);
  CLOSE();
  Check(&ERRORS);

  Generate(&Start);
  if (DoC) SHOW_STATES();
  while (1) {
    SetTables();
    struct Node* Nd = Parse();
    if (Position == 0) break;
    printf("\nParse Forest:\n");
    if (Nd != 0) {
      putchar('*'); SHOW_NODE(Nd - NodeTab); printf(".\n");
    }
    SHOW_FOREST();
    if (DoS) {
      printf("\nParse Stack:\n"), SHOW_STACK();
    }
    FreeTables();
  }
#endif

  return 0;
}

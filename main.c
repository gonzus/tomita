#include <stdio.h>
#include "util.h"
#include "log.h"
// #include "lex.h"
// #include "sym.h"
// #include "tomita.h"
#include "grammar.h"

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
  char buf[MAX_BUF];
  unsigned len = slurp_file(argv[Arg], buf, MAX_BUF);
  LOG_INFO("read %u bytes from [%s]", len, argv[Arg]);
  Slice text = slice_from_memory(buf, len);
  Grammar* grammar = grammar_create(text);
  unsigned errors = grammar_check(grammar);
  LOG_INFO("created grammar with %u errors", errors);
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

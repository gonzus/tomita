#include <stdio.h>
#include "lex.h"
#include "sym.h"
#include "tomita.h"

int main(int argc, char **argv) {
  int ERRORS = 0;
  Symbol Start; struct Node* Nd; int Arg; char *AP;
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
  if (!OPEN(argv[Arg])) {
    fprintf(stderr, "Cannot open %s.\n", argv[Arg]);
    return 1;
  }
  Start = Grammar(&ERRORS);
  CLOSE();
  Check(&ERRORS);
  Generate(Start);
  if (DoC) SHOW_STATES();
  while (1) {
    SetTables();
    Nd = Parse();
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
  return 0;
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "lex.h"

static char ChBuf[MAX_CHAR];
static FILE *InF = 0;

int LINE = 1;
const char* LastW = 0;
const char* ChArr = ChBuf;
char* ChP = ChBuf;

int OPEN(const char* path) {
  InF = fopen(path, "r");
  return InF != NULL;
}

int CLOSE(void) {
  if (InF == NULL) return 0;
  fclose(InF);
  InF = stdin;
  return 1;
}

int GET(void) {
  int Ch = fgetc(InF);
  if (Ch == '\n') LINE++;
  return Ch;
}

void UNGET(int Ch) {
  if (Ch == '\n') --LINE;
  ungetc(Ch, InF);
}

Lexical LEX(int *errors) {
  int Ch;
  do Ch = GET(); while (isspace(Ch));
  switch (Ch) {
    case EOF: return EndT;
    case '|': return BarT;
    case '*': return StarT;
    case ':': return ColonT;
    case '=': return EqualT;
    case '.': return DotT;
  }
  if (isalpha(Ch) || Ch == '_') {
    for (LastW = ChP; isalnum(Ch) || Ch == '_'; ChP++) {
      if (ChP - ChArr == MAX_CHAR)
        printf("Out of character space.\n"), exit(1);
      *ChP = Ch, Ch = GET();
    }
    if (Ch != EOF) UNGET(Ch);
    if (ChP - ChArr == MAX_CHAR) printf("Out of character space.\n"), exit(1);
    *ChP++ = '\0';
    return IdenT;
  } else if (Ch == '"') {
    Ch = GET();
    for (LastW = ChP; Ch != '"' && Ch != EOF; ChP++) {
      if (ChP - ChArr == MAX_CHAR)
        printf("Out of character space.\n"), exit(1);
      *ChP = Ch, Ch = GET();
    }
    if (Ch == EOF) printf("Missing closing \".\n"), exit(1);
    if (ChP - ChArr == MAX_CHAR) printf("Out of character space.\n"), exit(1);
    *ChP++ = '\0';
    return IdenT;
  } else {
    ERROR(LINE, errors, "extra character %c", Ch); return EndT;
  }
}

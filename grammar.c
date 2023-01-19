#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
// #include "util.h"
#include "grammar.h"

#define MAX_SYM 0x100

typedef enum { EndT, StarT, ColonT, EqualT, BarT, IdenT, DotT } TokenType;

typedef struct Token {
  TokenType typ;
  Slice val;
} Token;

static Symbol SymBuf[MAX_SYM];
static Symbol* SymP;

static unsigned LEX(Slice text, unsigned pos, Token* tok);

Grammar* grammar_create(Slice text) {
  Grammar* grammar = (Grammar*) malloc(sizeof(Grammar));
  memset(grammar, 0, sizeof(Grammar));

  unsigned pos = 0;
  Token tok = {0};
  int SawStart = 0;
  Symbol LHS = 0;
  pos = LEX(text, pos, &tok);

START:
  switch (tok.typ) {
    case EndT:
      return grammar;

    case IdenT:
      goto EQUAL;

    case DotT:
      pos = LEX(text, pos, &tok);
      goto START;

    case ColonT:
    case EqualT:
      // TODO ERROR(LINE, errors, "Missing left-hand side of rule, or '.' from previous rule.");
      goto FLUSH;

    case StarT:
      pos = LEX(text, pos, &tok);
      if (tok.typ != IdenT) {
        // TODO ERROR(LINE, errors, "Missing symbol after '*'.");
      } else {
        LookUpSlice(&grammar->start, tok.val, 0);
        if (SawStart++ > 0) {
          // TODO ERROR(LINE, errors, "Start symbol redefined.");
        }
        pos = LEX(text, pos, &tok);
      }
      goto FLUSH;

    case BarT:
      // TODO ERROR(LINE, errors, "Corrupt rule."); goto FLUSH;
      break;
  }

FLUSH:
  while (tok.typ != DotT && tok.typ != EndT)
    pos = LEX(text, pos, &tok);
  goto END;

EQUAL:
#if 1
  // TODO big motherfucking TODO
  LookUpSlice(&LHS, tok.val, 0);
  if (grammar->start == 0) grammar->start = LHS;
  pos = LEX(text, pos, &tok);
  LHS->Defined = 1;
  if (tok.typ == DotT) {
    SymP = SymBuf; *SymP++ = LHS, *SymP++ = 0;
    InsertR(sym_lookup(LHS->Name, 1));
  } else if (L == ColonT) {
    SymP = SymBuf; *SymP++ = LHS, *SymP++ = 0;
    for (L = LEX(errors); L == IdenT; L = LEX(errors)) InsertR(sym_lookup(LastW, 1));
  } else if (L == EqualT) {
    do {
      L = LEX(errors);
      for (SymP = SymBuf; L == IdenT && SymP < SymBuf + MAX_SYM; L = LEX(errors))
        *SymP++ = sym_lookup(LastW, 0);
      if (SymP >= SymBuf + MAX_SYM) printf("Large rule.\n"), exit(1);
      *SymP++ = 0; InsertR(LHS);
    } while (L == BarT);
  } else {
    ERROR(LINE, errors, "Missing '=' or ':'."); goto FLUSH;
  }
#endif

END:
  if (tok.typ == EndT) {
    // TODO ERROR(LINE, errors, "Missing '.'"); return; }
  }
  if (tok.typ == DotT) {
    pos = LEX(text, pos, &tok);
  }
  goto START;

  return grammar; // unreachable
}

void grammar_destroy(Grammar* grammar) {
  free(grammar);
}

static unsigned LEX(Slice text, unsigned pos, Token* tok) {
  memset(tok, 0, sizeof(Token));

  // skip whitespace
  while (pos < text.len && isspace(text.ptr[pos])) ++pos;

  // end of text? we are done
  if (pos >= text.len) {
    tok->typ = EndT;
    return pos;
  }

  // one of these special single characters?
  if (text.ptr[pos] == '|') {
    tok->typ = BarT;
    return pos;
  }
  if (text.ptr[pos] == '*') {
    tok->typ = StarT;
    return pos;
  }
  if (text.ptr[pos] == ':') {
    tok->typ = ColonT;
    return pos;
  }
  if (text.ptr[pos] == '=') {
    tok->typ = EqualT;
    return pos;
  }
  if (text.ptr[pos] == '.') {
    tok->typ = DotT;
    return pos;
  }

  // has to be a (possible quoted) identifier

  // an unquoted identifier?
  if (isalpha(text.ptr[pos]) || (text.ptr[pos] == '_')) {
    unsigned start = pos;
    while (pos < text.len && (isalnum(text.ptr[pos]) || text.ptr[pos] == '_')) ++pos;
    tok->typ = IdenT;
    tok->val = slice_from_memory(text.ptr + start, pos - start);
    return pos;
  }

  // a double-quoted identifier?
  if (text.ptr[pos] == '"') {
    ++pos;
    unsigned start = pos;
    while (pos < text.len && text.ptr[pos] != '"') ++pos;
    tok->typ = IdenT;
    tok->val = slice_from_memory(text.ptr + start, pos - start);
    ++pos;
    return pos;
  }

  // TODO: error out
  return pos;
}

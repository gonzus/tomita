#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "grammar.h"

#define MAX_SYM 0x100

typedef enum { EndT, StarT, ColonT, EqualT, BarT, IdenT, DotT } TokenType;

typedef struct Token {
  TokenType typ;
  Slice val;
} Token;

// A work buffer for pointers to symbols, used to store rules.
// Once a rule is completed, we make a call to symbol_insert_rule(), which
// copies all the pointers into rules, and then reset the work buffer.
static Symbol* sym_buf[MAX_SYM];
static Symbol** sym_pos;

static Symbol* lookup(Grammar* grammar, Slice name, unsigned literal);
static unsigned next_token(Slice text, unsigned pos, Token* tok);

Grammar* grammar_create(Slice text) {
  Grammar* grammar = (Grammar*) malloc(sizeof(Grammar));
  memset(grammar, 0, sizeof(Grammar));
  grammar->symtab = symtab_create();

  unsigned pos = 0;
  Token tok = {0};
  int saw_start = 0;
  Symbol* lhs = 0;
  pos = next_token(text, pos, &tok);

  // TODO: how to structure this without any gotos?
START:
  switch (tok.typ) {
    case EndT:
      return grammar;

    case IdenT:
      goto EQUAL;

    case DotT:
      pos = next_token(text, pos, &tok);
      goto START;

    case ColonT:
    case EqualT:
      LOG_WARN("missing left-hand side of rule, or '.' from previous rule");
      goto FLUSH;

    case StarT:
      pos = next_token(text, pos, &tok);
      if (tok.typ != IdenT) {
        LOG_WARN("missing symbol after '*'");
      } else {
        grammar->start = lookup(grammar, tok.val, 0);
        if (saw_start++ > 0) {
          LOG_WARN("start symbol redefined");
        }
        pos = next_token(text, pos, &tok);
      }
      goto FLUSH;

    case BarT:
      LOG_WARN("corrupt rule");
      goto FLUSH;
      break;
  }

FLUSH:
  while (tok.typ != DotT && tok.typ != EndT)
    pos = next_token(text, pos, &tok);
  goto END;

EQUAL:
  lhs = lookup(grammar, tok.val, 0);
  lhs->defined = 1;
  if (grammar->start == 0) {
    grammar->start = lhs;
  }
  pos = next_token(text, pos, &tok);
  switch (tok.typ) {
    case DotT:
      sym_pos = sym_buf;
      *sym_pos++ = lhs;
      *sym_pos++ = 0;
      symbol_insert_rule(lookup(grammar, lhs->name, 1), sym_buf, sym_pos);
      break;

    case ColonT:
      sym_pos = sym_buf;
      *sym_pos++ = lhs;
      *sym_pos++ = 0;
      for (pos = next_token(text, pos, &tok); tok.typ == IdenT; pos = next_token(text, pos, &tok)) {
        symbol_insert_rule(lookup(grammar, tok.val, 1), sym_buf, sym_pos);
      }
      break;

    case EqualT:
      do {
        pos = next_token(text, pos, &tok);
        for (sym_pos = sym_buf; tok.typ == IdenT && sym_pos < sym_buf + MAX_SYM; pos = next_token(text, pos, &tok)) {
          *sym_pos++ = lookup(grammar, tok.val, 0);
        }
        if (sym_pos >= sym_buf + MAX_SYM) {
          LOG_FATAL("rule too large, max is %u", MAX_SYM);
          exit(1);
        }
        *sym_pos++ = 0;
        symbol_insert_rule(lhs, sym_buf, sym_pos);
      } while (tok.typ == BarT);
      break;

    default:
      LOG_WARN("missing '=' or ':'");
      goto FLUSH;
      break;
  }

END:
  if (tok.typ == EndT) {
    LOG_WARN("missing '.'");
    return grammar;
  }
  if (tok.typ == DotT) {
    pos = next_token(text, pos, &tok);
  }
  goto START;

  return grammar; // unreachable
}

void grammar_destroy(Grammar* grammar) {
  symtab_destroy(grammar->symtab);
  free(grammar);
}

unsigned grammar_check(Grammar* grammar) {
  unsigned total = 0;
  unsigned errors = 0;
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->tail) {
    ++total;
    if (symbol->defined) continue;
    if (symbol->literal) continue;
    LOG_WARN("symbol [%.*s] undefined.\n", symbol->name.len, symbol->name.ptr);
    ++errors;
  }
  LOG_INFO("checked grammar: %u total symbols, %u errors", total, errors);
  return errors;
}

#if 0
// TODO
// This is the next step -- generating a parser from the grammar.
void grammar_generate(Symbol* Start) {
  struct Item* It;
  struct Item* *Its;
  struct Item* *QBuf;
  unsigned int S, X, Q, Qs, QMax; struct Items* QS;
  Rule StartR = Allocate(2 * sizeof(Symbol));
  StartR[0] = *Start, StartR[1] = 0;
  Its = Allocate(sizeof(struct Item*)), Its[0] = FormItem(0, StartR);
  SList = 0, STab = 0, Ss = 0; AddState(1, Its);
  XTab = 0, XMax = 0;
  QBuf = 0, QMax = 0;
  for (S = 0; S < Ss; S++) {
    unsigned ERs, RRs, E, R; unsigned char Final;
    QS = &STab[S];
    if (QS->Size > QMax)
      QMax = QS->Size, QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
    for (Qs = 0; Qs < QS->Size; Qs++)
      QBuf[Qs] = QS->List[Qs];
    for (ERs = RRs = 0, Final = 0, Xs = 0, Q = 0; Q < Qs; Q++) {
      It = QBuf[Q];
      if (*It->Pos == 0) {
        if (It->LHS == 0) Final++;
        else if (*It->RHS == 0) ERs++; else RRs++;
      } else {
        Symbol Pre = *It->Pos++; struct Items* IS = GetItem(Pre);
        if (IS->Size == 0) {
          if (Qs + Pre->Rules > QMax)
            QMax = Qs + Pre->Rules,
              QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
          for (R = 0; R < Pre->Rules; R++, Qs++)
            QBuf[Qs] = FormItem(Pre, Pre->RList[R]);
        }
        AddItem(IS, It);
        --It->Pos;
      }
    }
    MakeState(&SList[S], Final, ERs, RRs, Xs);
    for (E = R = 0, Q = 0; Q < Qs; Q++) {
      It = QBuf[Q];
      if (*It->Pos != 0 || It->LHS == 0) continue;
      if (*It->RHS == 0) SList[S].EList[E++] = It->LHS;
      else {
          struct Reduce* Rd = &SList[S].RList[R++];
          Rd->LHS = It->LHS, Rd->RHS = It->RHS;
        }
    }
    for (X = 0; X < Xs; X++) {
      struct Shift* Sh = &SList[S].SList[X];
      Sh->X = XTab[X].Pre, Sh->Q = AddState(XTab[X].Size, XTab[X].List);
    }
  }
  FREE(XTab);
  FREE(QBuf);
}
#endif

static Symbol* lookup(Grammar* grammar, Slice name, unsigned literal) {
  Symbol* symbol = 0;
  int created = symtab_lookup(grammar->symtab, name, literal, &symbol);
  if (!created) return symbol;

  if (grammar->first == 0) {
    grammar->first = symbol;
  } else {
    grammar->last->tail = symbol;
  }
  grammar->last = symbol;
  return symbol;
}

static unsigned next_token(Slice text, unsigned pos, Token* tok) {
  memset(tok, 0, sizeof(Token));
  do {
    // skip whitespace
    while (pos < text.len && isspace(text.ptr[pos])) ++pos;

    // end of text? we are done
    if (pos >= text.len) {
      tok->typ = EndT;
      break;
    }

    // one of these special single characters?
    if (text.ptr[pos] == '|') {
      tok->typ = BarT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == '*') {
      tok->typ = StarT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == ':') {
      tok->typ = ColonT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == '=') {
      tok->typ = EqualT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == '.') {
      tok->typ = DotT;
      ++pos;
      break;
    }

    // an unquoted identifier?
    if (isalpha(text.ptr[pos]) || (text.ptr[pos] == '_')) {
      unsigned beg = pos;
      while (pos < text.len && (isalnum(text.ptr[pos]) || text.ptr[pos] == '_')) ++pos;
      tok->typ = IdenT;
      tok->val = slice_from_memory(text.ptr + beg, pos - beg);
      break;
    }

    // a double-quoted identifier?
    if (text.ptr[pos] == '"') {
      if (text.ptr[pos] == '"') ++pos; // skip opening "
      unsigned beg = pos;
      while (pos < text.len && text.ptr[pos] != '"') ++pos;
      tok->typ = IdenT;
      tok->val = slice_from_memory(text.ptr + beg, pos - beg);
      if (text.ptr[pos] == '"') ++pos; // skip closing "
      break;
    }

  } while (0);

  LOG_DEBUG("token type %u, value [%.*s]", tok->typ, tok->val.len, tok->val.ptr);
  return pos;
}

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "mem.h"
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
static unsigned input_flush(Slice text, unsigned pos, Token* tok);
static unsigned input_token(Slice text, unsigned pos, Token* tok);

Grammar* grammar_create(Slice text) {
  Grammar* grammar = 0;
  MALLOC(Grammar, grammar);
  grammar->symtab = symtab_create();

  int saw_start = 0;
  unsigned pos = 0;
  Token tok = {0};
  pos = input_token(text, pos, &tok);
  for (int done = 0; !done;) {
    int do_equal = 0;
    switch (tok.typ) {
      case EndT:
        done = 1;
        continue;

      case IdenT:
        do_equal = 1;
        break;

      case DotT:
        pos = input_token(text, pos, &tok);
        continue;

      case ColonT:
      case EqualT:
        LOG_WARN("missing left-hand side of rule, or '.' from previous rule");
        pos = input_flush(text, pos, &tok);
        break;

      case StarT:
        pos = input_token(text, pos, &tok);
        if (tok.typ != IdenT) {
          LOG_WARN("missing symbol after '*'");
        } else {
          grammar->start = lookup(grammar, tok.val, 0);
          if (saw_start++ > 0) {
            LOG_WARN("start symbol redefined");
          }
          pos = input_token(text, pos, &tok);
        }
        pos = input_flush(text, pos, &tok);
        break;

      case BarT:
        LOG_WARN("corrupt rule");
        pos = input_flush(text, pos, &tok);
        break;
    }

    if (do_equal) {
      Symbol* lhs = lookup(grammar, tok.val, 0);
      lhs->defined = 1;
      if (grammar->start == 0) {
        grammar->start = lhs;
      }
      pos = input_token(text, pos, &tok);
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
          for (pos = input_token(text, pos, &tok); tok.typ == IdenT; pos = input_token(text, pos, &tok)) {
            symbol_insert_rule(lookup(grammar, tok.val, 1), sym_buf, sym_pos);
          }
          break;

        case EqualT:
          do {
            pos = input_token(text, pos, &tok);
            for (sym_pos = sym_buf; tok.typ == IdenT && sym_pos < sym_buf + MAX_SYM; pos = input_token(text, pos, &tok)) {
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
          pos = input_flush(text, pos, &tok);
          break;
      }
    }

    if (tok.typ == EndT) {
      LOG_WARN("missing '.'");
      break;
    }
    if (tok.typ == DotT) {
      pos = input_token(text, pos, &tok);
    }
  }

  return grammar;
}

void grammar_destroy(Grammar* grammar) {
  symtab_destroy(grammar->symtab);
  FREE(grammar);
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

static unsigned input_flush(Slice text, unsigned pos, Token* tok) {
  while (tok->typ != DotT && tok->typ != EndT)
    pos = input_token(text, pos, tok);
  return pos;
}

static unsigned input_token(Slice text, unsigned pos, Token* tok) {
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

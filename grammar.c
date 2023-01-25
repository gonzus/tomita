#include <ctype.h>
#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "grammar.h"

#define GRAMMAR_COMMENT    '#'
#define GRAMMAR_TERMINATOR ';'
#define GRAMMAR_EQ_RULE    ':'
#define GRAMMAR_EQ_TOKEN   '='
#define GRAMMAR_START      '@'
#define GRAMMAR_OR         '|'

typedef enum { EndT, StartT, EqTokenT, EqRuleT, OrT, IdenT, TermT } TokenType;

typedef struct Token {
  TokenType typ;
  Slice val;
} Token;

// A work buffer for pointers to symbols, used to store rules.
// Once a rule is completed, we make a call to symbol_insert_rule(), which
// copies all the pointers into rules, and then reset the work buffer.
#define MAX_SYM 0x100
static Symbol* sym_buf[MAX_SYM];
static Symbol** sym_pos;
static unsigned in_comment;

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

      case TermT:
        pos = input_token(text, pos, &tok);
        continue;

      case EqTokenT:
      case EqRuleT:
        LOG_WARN("missing left-hand side of rule, or '%c' from previous rule", GRAMMAR_TERMINATOR);
        pos = input_flush(text, pos, &tok);
        break;

      case StartT:
        pos = input_token(text, pos, &tok);
        if (tok.typ != IdenT) {
          LOG_WARN("missing symbol after '%c'", GRAMMAR_START);
        } else {
          grammar->start = lookup(grammar, tok.val, 0);
          if (saw_start++ > 0) {
            LOG_WARN("start symbol redefined");
          }
          pos = input_token(text, pos, &tok);
        }
        pos = input_flush(text, pos, &tok);
        break;

      case OrT:
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
        case TermT:
          sym_pos = sym_buf;
          *sym_pos++ = lhs;
          *sym_pos++ = 0;
          symbol_insert_rule(lookup(grammar, lhs->name, 1), sym_buf, sym_pos);
          break;

        case EqTokenT:
          sym_pos = sym_buf;
          *sym_pos++ = lhs;
          *sym_pos++ = 0;
          for (pos = input_token(text, pos, &tok); tok.typ == IdenT; pos = input_token(text, pos, &tok)) {
            symbol_insert_rule(lookup(grammar, tok.val, 1), sym_buf, sym_pos);
          }
          break;

        case EqRuleT:
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
          } while (tok.typ == OrT);
          break;

        default:
          LOG_WARN("missing '%c' or '%c'", GRAMMAR_EQ_RULE, GRAMMAR_EQ_TOKEN);
          pos = input_flush(text, pos, &tok);
          break;
      }
    }

    if (tok.typ == EndT) {
      LOG_WARN("missing '%c'", GRAMMAR_TERMINATOR);
      break;
    }
    if (tok.typ == TermT) {
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
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    ++total;
    if (symbol->literal) continue;
    if (symbol->defined) continue;
    LOG_WARN("symbol [%.*s] undefined.\n", symbol->name.len, symbol->name.ptr);
    ++errors;
  }
  LOG_INFO("checked grammar: %u total symbols, %u errors", total, errors);
  return errors;
}

static void pad(unsigned padding) {
  for (unsigned j = 0; j < padding; ++j) {
    printf("%c", ' ');
  }
}

static void show_symbol(Grammar* grammar, Symbol* symbol, int terminals) {
  if (!symbol->defined) {
    // this non-terminal was never the LHS of a rule
    printf("%.*s: UNDEFINED;\n", symbol->name.len, symbol->name.ptr);
    return;
  }

  if (symbol->rule_count && !terminals) {
    // this is a proper non-terminal with rules
    int padding = symbol->name.len + 1;
    printf("%.*s ", symbol->name.len, symbol->name.ptr);
    for (unsigned j = 0; j < symbol->rule_count; ++j) {
      if (j > 0) pad(padding);
      printf("%c", j == 0 ? ':' : '|');
      for (Symbol** rule = symbol->rules[j]; *rule; ++rule) {
        printf(" %.*s", (*rule)->name.len, (*rule)->name.ptr);
      }
      printf("\n");
    }
    pad(padding);
    printf(";\n");
    return;
  }

  if (!symbol->rule_count && terminals) {
    // this non-terminal is one of a list of tokens
    printf("%.*s =", symbol->name.len, symbol->name.ptr);
    for (Symbol* rhs = grammar->first; rhs != 0; rhs = rhs->nxt_list) {
      if (!rhs->literal) continue;
      for (unsigned j = 0; j < rhs->rule_count; ++j) {
        Symbol* back = *rhs->rules[j];
        if (!slice_equal(symbol->name, back->name)) continue;
        printf(" \"%.*s\"", rhs->name.len, rhs->name.ptr);
      }
    }
    printf(";\n");
  }
}

void grammar_show(Grammar* grammar) {
  printf("# start\n");
  printf("@ %.*s\n", grammar->start->name.len, grammar->start->name.ptr);

  printf("\n# rules\n");
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 0);
  }

  printf("\n# terminals\n");
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 1);
  }
}

static Symbol* lookup(Grammar* grammar, Slice name, unsigned literal) {
  Symbol* symbol = 0;
  int created = symtab_lookup(grammar->symtab, name, literal, &symbol);
  if (!created) return symbol;

  if (grammar->first == 0) {
    grammar->first = symbol;
  } else {
    grammar->last->nxt_list = symbol;
  }
  grammar->last = symbol;
  return symbol;
}

static unsigned input_flush(Slice text, unsigned pos, Token* tok) {
  while (tok->typ != TermT && tok->typ != EndT)
    pos = input_token(text, pos, tok);
  return pos;
}

static unsigned input_token(Slice text, unsigned pos, Token* tok) {
  memset(tok, 0, sizeof(Token));
  do {
    // skip comments
    if (in_comment) {
      while (pos < text.len && text.ptr[pos] != '\n') ++pos;
      ++pos;
      in_comment = 0;
      continue;
    }

    // skip whitespace
    while (pos < text.len && isspace(text.ptr[pos])) ++pos;

    // end of text? we are done
    if (pos >= text.len) {
      tok->typ = EndT;
      break;
    }

    // one of these special single characters?
    if (text.ptr[pos] == GRAMMAR_COMMENT) {
      in_comment = 1;
      ++pos;
      continue;
    }
    if (text.ptr[pos] == GRAMMAR_OR) {
      tok->typ = OrT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == GRAMMAR_START) {
      tok->typ = StartT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == GRAMMAR_EQ_TOKEN) {
      tok->typ = EqTokenT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == GRAMMAR_EQ_RULE) {
      tok->typ = EqRuleT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == GRAMMAR_TERMINATOR) {
      tok->typ = TermT;
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
  } while (1);

  LOG_DEBUG("token type %u, value [%.*s]", tok->typ, tok->val.len, tok->val.ptr);
  return pos;
}

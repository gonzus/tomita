#include <ctype.h>
#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "util.h"
#include "tomita.h"
#include "symbol.h"
#include "symtab.h"
#include "grammar.h"

/* Input format derived from the syntax:
   Grammar = Rule+.
   Rule = "*" ID "."
        | ID "."
        | ID "=" ID* ("|" ID*)* "."
        | ID ":" ID+ "."
        .
 */

typedef enum { EndT, StartT, EqTokenT, EqRuleT, OrT, IdenT, TermT } TokenType;

typedef struct Token {
  TokenType typ;
  Slice val;
} Token;

// A work buffer for pointers to symbols, used to store rules.
// Once a rule is completed, we make a call to symbol_insert_rule(), which
// copies all the pointers into rules, and then reset the work buffer.
enum {
  MAX_SYM = 0x100,
};
static Symbol* sym_buf[MAX_SYM];
static Symbol** sym_pos;
static unsigned in_comment;

static unsigned grammar_check(Grammar* grammar);
static void pad(unsigned padding);
static void show_symbol(Grammar* grammar, Symbol* symbol, int terminals);
static unsigned input_flush(Slice text, unsigned pos, Token* tok);
static unsigned input_token(Slice text, unsigned pos, Token* tok);

Grammar* grammar_create(SymTab* symtab) {
  Grammar* grammar = 0;
  MALLOC(Grammar, grammar);
  buffer_build(&grammar->source);
  grammar->symtab = symtab;
  return grammar;
}

void grammar_destroy(Grammar* grammar) {
  grammar_clear(grammar);
  buffer_destroy(&grammar->source);
  FREE(grammar);
}

void grammar_clear(Grammar* grammar) {
  buffer_clear(&grammar->source);
  symtab_clear(grammar->symtab);
  grammar->start = 0;
}

void grammar_show(Grammar* grammar) {
  printf("%c%c GRAMMAR\n", FORMAT_COMMENT, FORMAT_COMMENT);
  if (grammar->start) {
    printf("%c start\n", FORMAT_COMMENT);
    printf("@ %.*s\n", grammar->start->name.len, grammar->start->name.ptr);
  }

  printf("\n%c rules\n", FORMAT_COMMENT);
  for (Symbol* symbol = grammar->symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 0);
  }

  printf("\n%c terminals\n", FORMAT_COMMENT);
  for (Symbol* symbol = grammar->symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 1);
  }
}

unsigned grammar_compile_from_slice(Grammar* grammar, Slice source) {
  grammar_clear(grammar);
  buffer_append_slice(&grammar->source, source);
  Slice text = buffer_slice(&grammar->source);

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
          grammar->start = symtab_lookup(grammar->symtab, tok.val, 0, 1);
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
      Symbol* lhs = symtab_lookup(grammar->symtab, tok.val, 0, 1);
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
          symbol_insert_rule(symtab_lookup(grammar->symtab, lhs->name, 1, 1), sym_buf, sym_pos, &grammar->symtab->rules_counter, 0);
          break;

        case EqTokenT:
          sym_pos = sym_buf;
          *sym_pos++ = lhs;
          *sym_pos++ = 0;
          for (pos = input_token(text, pos, &tok); tok.typ == IdenT; pos = input_token(text, pos, &tok)) {
            symbol_insert_rule(symtab_lookup(grammar->symtab, tok.val, 1, 1), sym_buf, sym_pos, &grammar->symtab->rules_counter, 0);
          }
          break;

        case EqRuleT:
          do {
            pos = input_token(text, pos, &tok);
            for (sym_pos = sym_buf; tok.typ == IdenT && sym_pos < sym_buf + MAX_SYM; pos = input_token(text, pos, &tok)) {
              *sym_pos++ = symtab_lookup(grammar->symtab, tok.val, 0, 1);
            }
            if (sym_pos >= sym_buf + MAX_SYM) {
              LOG_FATAL("rule too large, max is %u", MAX_SYM);
              exit(1);
            }
            *sym_pos++ = 0;
            symbol_insert_rule(lhs, sym_buf, sym_pos, &grammar->symtab->rules_counter, 0);
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

  return grammar_check(grammar);
}

unsigned grammar_load_from_slice(Grammar* grammar, Slice source) {
  grammar_clear(grammar);
  buffer_append_slice(&grammar->source, source);
  Slice text = buffer_slice(&grammar->source);

  do {
    symtab_load_from_slice(grammar->symtab, &text);

    SliceLookup lookup_lines = {0};
    while (slice_tokenize_by_byte(text, '\n', &lookup_lines)) {
      Slice line = slice_trim(lookup_lines.result);
      LOG_DEBUG("read line [%.*s]", line.len, line.ptr);
      char lead = line.ptr[0];
      unsigned pos = 0;
      if (lead == FORMAT_COMMENT) continue;
      ++pos;

      if (lead == FORMAT_GRAMMAR) {
        unsigned index = 0;
        pos = next_number(line, pos, &index);
        Symbol* start = symtab_find_symbol_by_index(grammar->symtab, index);
        assert(start);
        grammar->start = start;
        LOG_DEBUG("loaded grammar: start=%u", index);
        continue;
      }
      // found something else
      unsigned used = line.ptr - text.ptr;
      text.ptr += used;
      text.len -= used;
    }
  } while (0);

  return 0;
}

unsigned grammar_save_to_buffer(Grammar* grammar, Buffer* b) {
  unsigned errors = 0;
  do {
    errors = symtab_save_to_buffer(grammar->symtab, b);
    if (errors) break;
    buffer_format_print(b, "%c grammar: start\n", FORMAT_COMMENT);
    buffer_format_print(b, "%c %u\n", FORMAT_GRAMMAR, grammar->start->index);
  } while (0);
  return errors;
}

static unsigned grammar_check(Grammar* grammar) {
  unsigned total = 0;
  unsigned errors = 0;
  for (Symbol* symbol = grammar->symtab->first; symbol != 0; symbol = symbol->nxt_list) {
    ++total;
    if (symbol->literal) continue;
    if (symbol->defined) continue;
    LOG_WARN("symbol [%.*s] undefined.\n", symbol->name.len, symbol->name.ptr);
    ++errors;
  }
  LOG_DEBUG("checked grammar: %u total symbols, %u errors", total, errors);
  UNUSED(total);
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
    printf("%.*s%c UNDEFINED;\n", symbol->name.len, symbol->name.ptr, GRAMMAR_EQ_RULE);
    return;
  }

  if (symbol->rs_cap && !terminals) {
    // this is a proper non-terminal with rules
    int padding = symbol->name.len + 1;
    for (unsigned j = 0; j < symbol->rs_cap; ++j) {
      RuleSet* rs = &symbol->rs_table[j];
      printf("%3u: ", rs->index);
      if (j == 0) {
        printf("%.*s ", symbol->name.len, symbol->name.ptr);
      } else {
        pad(padding);
      }
      printf("%c", j == 0 ? GRAMMAR_EQ_RULE : GRAMMAR_OR);
      for (Symbol** rule = rs->rules; *rule; ++rule) {
        printf(" %.*s", (*rule)->name.len, (*rule)->name.ptr);
      }
      printf("\n");
    }
    pad(5 + padding);
    printf("%c\n", GRAMMAR_TERMINATOR);
    return;
  }

  if (!symbol->rs_cap && terminals) {
    // this non-terminal is one of a list of tokens
    printf("%.*s %c", symbol->name.len, symbol->name.ptr, GRAMMAR_EQ_TOKEN);
    for (Symbol* rhs = grammar->symtab->first; rhs != 0; rhs = rhs->nxt_list) {
      if (!rhs->literal) continue;
      for (unsigned j = 0; j < rhs->rs_cap; ++j) {
        Symbol* back = *rhs->rs_table[j].rules;
        if (!slice_equal(symbol->name, back->name)) continue;
        printf(" \"%.*s\"", rhs->name.len, rhs->name.ptr);
      }
    }
    printf("%c\n", GRAMMAR_TERMINATOR);
  }
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
    if (text.ptr[pos] == FORMAT_COMMENT) {
      in_comment = 1;
      ++pos;
      continue;
    }
    if (text.ptr[pos] == GRAMMAR_OR) {
      tok->typ = OrT;
      ++pos;
      break;
    }
    if (text.ptr[pos] == GRAMMAR_SLASH) {
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
    if (pos + 1 < text.len && text.ptr[pos+0] == GRAMMAR_LT && text.ptr[pos+1] == GRAMMAR_MINUS) {
      tok->typ = EqRuleT;
      pos += 2;
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

    // a single-quoted identifier?
    if (text.ptr[pos] == '\'') {
      if (text.ptr[pos] == '\'') ++pos; // skip opening '
      unsigned beg = pos;
      while (pos < text.len && text.ptr[pos] != '\'') ++pos;
      tok->typ = IdenT;
      tok->val = slice_from_memory(text.ptr + beg, pos - beg);
      if (text.ptr[pos] == '\'') ++pos; // skip closing "
      break;
    }
  } while (1);

  LOG_DEBUG("token type %u, value [%.*s]", tok->typ, tok->val.len, tok->val.ptr);
  return pos;
}

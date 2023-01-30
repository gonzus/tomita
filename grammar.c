#include <ctype.h>
#include "log.h"
#include "mem.h"
#include "util.h"
#include "tomita.h"
#include "grammar.h"

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
static void grammar_reset(Grammar* grammar);
static unsigned grammar_check(Grammar* grammar);
static void pad(unsigned padding);
static void show_symbol(Grammar* grammar, Symbol* symbol, int terminals);
static unsigned next_number(Slice line, unsigned pos, unsigned* number);
static unsigned next_string(Slice line, unsigned pos, Slice* string);
Symbol* find_symbol_by_index(Grammar* grammar, unsigned index);

Grammar* grammar_create(void) {
  Grammar* grammar = 0;
  MALLOC(Grammar, grammar);
  buffer_build(&grammar->source);
  grammar->symtab = symtab_create();
  return grammar;
}

void grammar_destroy(Grammar* grammar) {
  symtab_destroy(grammar->symtab);
  buffer_destroy(&grammar->source);
  FREE(grammar);
}

void grammar_show(Grammar* grammar) {
  printf("%c start\n", FORMAT_COMMENT);
  printf("@ %.*s\n", grammar->start->name.len, grammar->start->name.ptr);

  printf("\n%c rules\n", FORMAT_COMMENT);
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 0);
  }

  printf("\n%c terminals\n", FORMAT_COMMENT);
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->literal) continue;
    show_symbol(grammar, symbol, 1);
  }
}

unsigned grammar_build_from_text(Grammar* grammar, Slice source) {
  grammar_reset(grammar);
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

  return grammar_check(grammar);
}

unsigned grammar_save_to_stream(Grammar* grammar, FILE* fp) {
  unsigned total_symbols = 0;
  unsigned total_rules = 0;
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    total_symbols += 1;
    total_rules += symbol->rule_count;
  }

  fprintf(fp, "%c grammar%c num_symbols num_rules start\n", FORMAT_COMMENT, GRAMMAR_EQ_RULE);
  fprintf(fp, "%c %u %u %u\n", FORMAT_GRAMMAR, total_symbols, total_rules, grammar->start->index);
  fprintf(fp, "%c symbols (%u)%c index name literal defined\n", FORMAT_COMMENT, total_symbols, GRAMMAR_EQ_RULE);
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    symbol_print_definition(symbol, fp);
  }
  fprintf(fp, "%c rules (%u)%c lhs [rhs...]\n", FORMAT_COMMENT, total_rules, GRAMMAR_EQ_RULE);
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    symbol_print_rules(symbol, fp);
  }

  return 0;
}

unsigned grammar_load_from_stream(Grammar* grammar, FILE* fp) {
  grammar_reset(grammar);

  do {
    unsigned len = slurp_stream(fp, &grammar->source);
    if (!len) break;

    unsigned total_symbols = 0;
    unsigned total_rules = 0;
    unsigned start_index = 0;
    unsigned s_seq = 0;
    Symbol* prev = 0;

    Slice text = buffer_slice(&grammar->source);
    SliceLookup lookup_lines = {0};
    while (slice_tokenize_by_byte(text, '\n', &lookup_lines)) {
      Slice line = slice_trim(lookup_lines.result);
      LOG_DEBUG("read line [%.*s]", line.len, line.ptr);
      char lead = line.ptr[0];
      if (lead == FORMAT_COMMENT) continue;

      unsigned pos = 1;
      if (lead == FORMAT_GRAMMAR) {
        pos = next_number(line, pos, &total_symbols);
        pos = next_number(line, pos, &total_rules);
        pos = next_number(line, pos, &start_index);
        continue;
      }
      if (lead == FORMAT_SYMBOL) {
        unsigned index = 0;
        Slice name;
        unsigned literal = 0;
        unsigned defined = 0;
        // unsigned rule_count = 0;
        pos = next_number(line, pos, &index);
        assert(index == s_seq++);
        pos = next_string(line, pos, &name);
        pos = next_number(line, pos, &literal);
        pos = next_number(line, pos, &defined);
        // pos = next_number(line, pos, &rule_count);
        Symbol* sym = 0;
        symtab_lookup(grammar->symtab, name, literal, &sym);
        assert(index == sym->index);
        sym->defined = defined;
        // sym->rule_count = rule_count;
        if (index == start_index) grammar->start = sym;
        if (!grammar->first) grammar->first = sym;
        grammar->last = sym;
        if (prev) prev->nxt_list = sym;
        prev = sym;
        continue;
      }
      if (lead == FORMAT_RULE) {
        unsigned lhs_index = 0;
        pos = next_number(line, pos, &lhs_index);
        Symbol* lhs = find_symbol_by_index(grammar, lhs_index);
        assert(lhs);
        sym_pos = sym_buf;
        while (1) {
          unsigned rhs_index = 0;
          pos = next_number(line, pos, &rhs_index);
          if (pos == 0) break;
          *sym_pos++ = find_symbol_by_index(grammar, rhs_index);
        }
        *sym_pos++ = 0;
        symbol_insert_rule(lhs, sym_buf, sym_pos);
      }
    }
  } while (0);

  return 0;
}

static unsigned grammar_check(Grammar* grammar) {
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
    printf("%.*s%c UNDEFINED;\n", symbol->name.len, symbol->name.ptr, GRAMMAR_EQ_RULE);
    return;
  }

  if (symbol->rule_count && !terminals) {
    // this is a proper non-terminal with rules
    int padding = symbol->name.len + 1;
    printf("%.*s ", symbol->name.len, symbol->name.ptr);
    for (unsigned j = 0; j < symbol->rule_count; ++j) {
      if (j > 0) pad(padding);
      printf("%c", j == 0 ? GRAMMAR_EQ_RULE : GRAMMAR_OR);
      for (Symbol** rule = symbol->rules[j]; *rule; ++rule) {
        printf(" %.*s", (*rule)->name.len, (*rule)->name.ptr);
      }
      printf("\n");
    }
    pad(padding);
    printf("%c\n", GRAMMAR_TERMINATOR);
    return;
  }

  if (!symbol->rule_count && terminals) {
    // this non-terminal is one of a list of tokens
    printf("%.*s %c", symbol->name.len, symbol->name.ptr, GRAMMAR_EQ_TOKEN);
    for (Symbol* rhs = grammar->first; rhs != 0; rhs = rhs->nxt_list) {
      if (!rhs->literal) continue;
      for (unsigned j = 0; j < rhs->rule_count; ++j) {
        Symbol* back = *rhs->rules[j];
        if (!slice_equal(symbol->name, back->name)) continue;
        printf(" \"%.*s\"", rhs->name.len, rhs->name.ptr);
      }
    }
    printf("%c\n", GRAMMAR_TERMINATOR);
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

static void grammar_reset(Grammar* grammar) {
  symtab_destroy(grammar->symtab);
  grammar->symtab = symtab_create();
  buffer_clear(&grammar->source);
  grammar->start = grammar->first = grammar->last = 0;
}

static unsigned skip_spaces(Slice line, unsigned pos) {
  while (pos < line.len && isspace(line.ptr[pos])) ++pos;
  return pos;
}

static unsigned next_number(Slice line, unsigned pos, unsigned* number) {
  *number = 0;
  pos = skip_spaces(line, pos);
  unsigned digits = 0;
  while (pos < line.len && isdigit(line.ptr[pos])) {
    *number *= 10;
    *number += line.ptr[pos] - '0';
    ++pos;
    ++digits;
  }
  LOG_DEBUG("next number=%u, digits=%u, pos=%u", *number, digits, pos);
  return digits > 0 ? pos : 0;
}

static unsigned next_string(Slice line, unsigned pos, Slice* string) {
  string->ptr = 0;
  string->len = 0;
  pos = skip_spaces(line, pos);
  while (pos < line.len) {
    if (line.ptr[pos] == '[') {
      ++pos;
      if (pos < line.len) string->ptr = &line.ptr[pos];
      continue;
    }
    if (line.ptr[pos] == ']') {
      string->len = &line.ptr[pos] - string->ptr;
      ++pos;
      break;
    }
    ++pos;
  }
  LOG_DEBUG("next string=<%.*s>, pos=%u", string->len, string->ptr, pos);
  return pos;
}

Symbol* find_symbol_by_index(Grammar* grammar, unsigned index) {
  // TODO: make this more efficient
  for (Symbol* symbol = grammar->first; symbol != 0; symbol = symbol->nxt_list) {
    if (symbol->index == index) {
      LOG_DEBUG("found symbol with index %u: %p [%.*s]", index, symbol, symbol->name.len, symbol->name.ptr);
      return symbol;
    }
  }
  return 0;
}

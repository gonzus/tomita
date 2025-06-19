#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "util.h"
#include "grammar.h"
#include "tomita.h"
#include "symtab.h"
#include "parser.h"

// a reference-counted Item containing:
//   a LHS symbol
//   a RHS ruleset
//   a pointer to the current symbol in the ruleset being considered
struct Item {
  Symbol* lhs;               // left-hand side symbol
  RuleSet rs;                // ruleset in right-hand side
  Symbol** rhs_pos;          // pointer to "current" symbol
  unsigned ref_cnt;          // reference count
};

// a list of Item pointers, with a Pre symbol
// TODO: what is Pre?
struct Items {
  Symbol* Pre;               // ???
  struct Item** item_table;  // Item table
  unsigned item_cap;         //   capacity of table
};

static struct Item* item_make(Parser* parser, Symbol* LHS, Symbol** RHS, unsigned index);
static struct Item* item_clone(struct Item* It);
static void item_add(struct Items* Its, struct Item* It);
static int item_compare(struct Item* l, struct Item* r);
static struct Items* items_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax);

static void state_make(struct ParserState* state, unsigned char final, unsigned er_new, unsigned rr_new, unsigned ss_new);
static int state_add(Parser* parser, struct Items** items_table, unsigned int Size, struct Item** List);

Parser* parser_create(SymTab* symtab) {
  Parser* parser = 0;
  MALLOC(Parser, parser);
  buffer_build(&parser->source);
  parser->symtab = symtab;
  return parser;
}

void parser_destroy(Parser* parser) {
  parser_clear(parser);
  buffer_destroy(&parser->source);
  FREE(parser);
}

void parser_clear(Parser* parser) {
  if (parser->states) {
    for (unsigned j = 0; j < parser->state_cap; ++j) {
      struct ParserState* state = &parser->states[j];
      FREE(state->er_table);
      FREE(state->rr_table);
      FREE(state->ss_table);
    }
    FREE (parser->states);
  }
  buffer_clear(&parser->source);
  parser->states = 0;
  parser->state_cap = 0;
}

unsigned parser_build_from_grammar(Parser* parser, Grammar* grammar) {
  parser_clear(parser);
  parser->symtab = grammar->symtab;

  // Create initial state
  Symbol** StartR = 0;
  MALLOC_N(Symbol*, StartR, 2);
  StartR[0] = grammar->start;
  StartR[1] = 0;

  struct Item** Its = 0;
  MALLOC(struct Item*, Its);
  Its[0] = item_make(parser, 0, StartR, 666);

  struct Items* items_table = 0;
  state_add(parser, &items_table, 1, Its);

  struct Items* XTab = 0;
  unsigned XMax = 0;

  struct Item** QBuf = 0;
  unsigned QMax = 0;
  unsigned Qs = 0;

  for (unsigned S = 0; S < parser->state_cap; ++S) {
    struct Items* QS = &items_table[S];
    if (QS->item_cap > QMax) {
      QMax = QS->item_cap;
      REALLOC(struct Item*, QBuf, QMax);
    }
    for (Qs = 0; Qs < QS->item_cap; ++Qs) {
      QBuf[Qs] = REF(QS->item_table[Qs]);
    }
    unsigned ERs = 0;
    unsigned RRs = 0;
    unsigned char final = 0;
    unsigned Xs = 0;
    for (unsigned Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->rhs_pos == 0) {
        if (It->lhs == 0) {
          ++final;
        }
        else if (*It->rs.rules == 0) {
          ++ERs;
        } else {
          ++RRs;
        }
        continue;
      }

      Symbol* Pre = *It->rhs_pos++;
      struct Items* IS = items_get(Pre, &XTab, &Xs, &XMax);
      if (IS->item_cap == 0) {
        if ((Qs + Pre->rs_cap) > QMax) {
          QMax = Qs + Pre->rs_cap;
          REALLOC(struct Item*, QBuf, QMax);
        }
        for (unsigned R = 0; R < Pre->rs_cap; ++R, ++Qs) {
          QBuf[Qs] = item_make(parser, Pre, Pre->rs_table[R].rules, Pre->rs_table[R].index);
        }
      }
      item_add(IS, It);
      --It->rhs_pos;
    }
    state_make(&parser->states[S], final, ERs, RRs, Xs);
    unsigned R = 0;
    unsigned E = 0;
    for (unsigned Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->rhs_pos != 0 || It->lhs == 0) continue;
      if (*It->rs.rules == 0) {
        parser->states[S].er_table[E++] = It->lhs;
      } else {
        struct Reduce* Rd = &parser->states[S].rr_table[R++];
        Rd->lhs = It->lhs;
        Rd->rs = It->rs;
      }
    }
    for (unsigned X = 0; X < Xs; ++X) {
      struct Shift* Sh = &parser->states[S].ss_table[X];
      Sh->symbol = XTab[X].Pre;
      Sh->state = state_add(parser, &items_table, XTab[X].item_cap, XTab[X].item_table);
    }
    for (unsigned Q = 0; Q < Qs; ++Q) {
        UNREF(QBuf[Q]);
    }
  }
  FREE(XTab);
  FREE(QBuf);
  FREE(StartR);

  if (items_table) {
    for (unsigned j = 0; j < parser->state_cap; ++j) {
      struct Items* items = &items_table[j];
      for (unsigned k = 0; k < items->item_cap; ++k) {
        UNREF(items->item_table[k]);
      }
      FREE(items->item_table);
    }
    FREE(items_table);
  }

  return 0;
}

void parser_show(Parser* parser) {
  printf("%c%c PARSER\n", FORMAT_COMMENT, FORMAT_COMMENT);
  printf("%c         accept:  ACCEPT seq\n", FORMAT_COMMENT);
  printf("%c   normal shift:  t => state\n", FORMAT_COMMENT);
  printf("%c epsilon reduce:  [A -> state]\n", FORMAT_COMMENT);
  printf("%c  normal reduce:  [A => B t C]\n", FORMAT_COMMENT);
  printf("%c           goto:  A goto state\n", FORMAT_COMMENT);
  unsigned conflict_sr = 0;
  unsigned conflict_rr = 0;
  for (unsigned S = 0; S < parser->state_cap; ++S) {
    struct ParserState* St = &parser->states[S];
    printf("%d:\n", S);

    unsigned accept_count = 0;
    unsigned reduce_count = 0;
    unsigned shift_count = 0;
    for (unsigned pass = 0; pass < 5; ++pass) {
      switch (pass) {
        case 0:
          if (St->final) {
            ++accept_count;
            printf("\tACCEPT %u\n", accept_count);
          }
          break;

        case 2:
          for (unsigned j = 0; j < St->er_cap; ++j) {
            Symbol* lhs = St->er_table[j];
            Slice name = lhs->name;
            printf("\t[%.*s -> 1]\n", name.len, name.ptr);
            ++reduce_count;
          }
          break;

        case 3:
          for (unsigned j = 0; j < St->rr_cap; ++j) {
            struct Reduce* reduce = &St->rr_table[j];
            Symbol* lhs = reduce->lhs;
            Slice ln = lhs->name;
            printf("\t[%.*s =>", ln.len, ln.ptr);
            for (Symbol** rhs = reduce->rs.rules; *rhs != 0; ++rhs) {
              Slice rn = (*rhs)->name;
              printf(" %.*s", rn.len, rn.ptr);
            }
            printf("]\n");
            ++reduce_count;
          }
          if (shift_count > 0 && reduce_count > 0) {
            unsigned n = shift_count * reduce_count;
            printf("\t\t*** %u shift/reduce conflict%s ***\n", n, n == 1 ? "" : "s");
            conflict_sr += n;
          }
          if (reduce_count > 1) {
            unsigned n = reduce_count - 1;
            printf("\t\t*** %u reduce/reduce conflict%s ***\n", n, n == 1 ? "" : "s");
            conflict_rr += n;
          }
          break;

        case 1:
          for (unsigned j = 0; j < St->ss_cap; ++j) {
            struct Shift* shift = &St->ss_table[j];
            Symbol* symbol = shift->symbol;
            Slice name = symbol->name;
            unsigned num_shift = symbol->literal ? 1 : !symbol->rs_cap;
            if (num_shift > 0) {
              printf("\t%.*s => %d\n", name.len, name.ptr, shift->state);
              shift_count += num_shift;
            }
          }
          break;

        case 4:
          for (unsigned j = 0; j < St->ss_cap; ++j) {
            struct Shift* shift = &St->ss_table[j];
            Symbol* symbol = shift->symbol;
            Slice name = symbol->name;
            unsigned num_shift = symbol->literal ? 1 : !symbol->rs_cap;
            if (num_shift == 0) {
              printf("\t%.*s goto %d\n", name.len, name.ptr, shift->state);
            }
          }
          break;
      }
    }
  }
  printf("--------\n");
  printf("%u total shift/reduce conflicts\n", conflict_sr);
  printf("%u total reduce/reduce conflicts\n", conflict_rr);
}

unsigned parser_load_from_slice(Parser* parser, Slice source) {
  parser_clear(parser);
  buffer_append_slice(&parser->source, source);
  Slice text = buffer_slice(&parser->source);

  do {
    LOG_DEBUG("LOADING parser from slice");
    symtab_load_from_slice(parser->symtab, &text);
    LOG_DEBUG("LOADED symtab, text now:\n%.*s\n", text.len, text.ptr);

    unsigned state_cap = 0;
    unsigned ss_cap = 0;
    unsigned rr_cap = 0;
    unsigned er_cap = 0;
    unsigned state_tot = 0;
    SliceLookup lookup_lines = {0};
    while (slice_tokenize_by_byte(text, '\n', &lookup_lines)) {
      Slice line = slice_trim(lookup_lines.result);
      LOG_DEBUG("read line [%.*s]", line.len, line.ptr);
      char lead = line.ptr[0];
      unsigned pos = 0;
      if (lead == FORMAT_COMMENT) continue;
      ++pos;

      if (lead == FORMAT_PARSER) {
        pos = next_number(line, pos, &state_cap);
        LOG_DEBUG("loaded parser: state_cap=%u", state_cap);

        // preallocate state table entries
        state_tot = parser->state_cap;
        while (state_tot < state_cap) {
          LOG_DEBUG("current cap=%u, need %u", state_tot, state_cap);
          TABLE_CHECK_GROW(parser->states, state_tot, 8, struct ParserState);
          state_tot += 8;
        }
        state_tot = 0;
        continue;
      }
      if (lead == FORMAT_STATE) {
        ++state_tot;
        unsigned final = 0;
        pos = next_number(line, pos, &final);
        pos = next_number(line, pos, &ss_cap);
        pos = next_number(line, pos, &rr_cap);
        pos = next_number(line, pos, &er_cap);
        LOG_DEBUG("loaded state: final=%u, ss=%u, rr=%u, er=%u", final, ss_cap, rr_cap, er_cap);
        state_make(&parser->states[state_tot-1], final, er_cap, rr_cap, ss_cap);
        parser->states[state_tot-1].ss_cap = 0;
        parser->states[state_tot-1].rr_cap = 0;
        parser->states[state_tot-1].er_cap = 0;
        continue;
      }
      if (lead == FORMAT_SHIFT) {
        int t = parser->states[state_tot-1].ss_cap++;
        unsigned index = 0;
        unsigned state = 0;
        pos = next_number(line, pos, &index);
        pos = next_number(line, pos, &state);
        LOG_DEBUG("loaded shift: index=%u, state=%u", index, state);
        struct Shift* shift = &parser->states[state_tot-1].ss_table[t];
        shift->symbol = symtab_find_symbol_by_index(parser->symtab, index);
        shift->state = state;
        continue;
      }
      if (lead == FORMAT_REDUCE) {
        int t = parser->states[state_tot-1].rr_cap++;
        unsigned lhs_index = 0;
        unsigned rs_index = 0;
        pos = next_number(line, pos, &lhs_index);
        pos = next_number(line, pos, &rs_index);
        LOG_DEBUG("loaded reduce: lhs=%u rs=%u", lhs_index, rs_index);
        struct Reduce* reduce = &parser->states[state_tot-1].rr_table[t];
        Symbol* lhs = symtab_find_symbol_by_index(parser->symtab, lhs_index);
        assert(lhs);
#if 1
        // TODO: get from hash table
        RuleSet* rs = symbol_find_ruleset_by_index(lhs, rs_index);
#else
        Number* num = numtab_lookup(parser->idx2rs, rs_index, 0);
        LOG_DEBUG("GONZO: parser %p: got num %p for index %u", parser, num, rs_index);
        assert(num);
        RuleSet* rs = num->ptr;
#endif
        assert(rs);
        reduce->lhs = lhs;
        reduce->rs = *rs;
        continue;
      }
      if (lead == FORMAT_EPSILON) {
        int t = parser->states[state_tot-1].er_cap++;
        unsigned index = 0;
        pos = next_number(line, pos, &index);
        LOG_DEBUG("loaded epsilon: index=%u", index);
        struct Symbol** epsilon = &parser->states[state_tot-1].er_table[t];
        Symbol* symbol = symtab_find_symbol_by_index(parser->symtab, index);
        *epsilon = symbol;
        continue;
      }
      // found something else
      unsigned used = line.ptr - text.ptr;
      text.ptr += used;
      text.len -= used;
      break;
    }
    parser->state_cap = state_cap;
  } while (0);

  return 0;
}

unsigned parser_save_to_buffer(Parser* parser, Buffer* b) {
  unsigned errors = 0;
  do {
    errors = symtab_save_to_buffer(parser->symtab, b);
    if (errors) break;

    buffer_format_print(b, "%c parser: table_size\n", FORMAT_COMMENT);
    buffer_format_print(b, "%c %u\n", FORMAT_PARSER, parser->state_cap);;
    buffer_format_print(b, "%c state (%u): final num_sa num_rr num_er\n", FORMAT_COMMENT, parser->state_cap);
    buffer_format_print(b, "%c   shift: symbol state\n", FORMAT_COMMENT);
    buffer_format_print(b, "%c   reduce: lhs rule\n", FORMAT_COMMENT);
    buffer_format_print(b, "%c   epsilon: symbol\n", FORMAT_COMMENT);
    for (unsigned j = 0; j < parser->state_cap; ++j) {
      struct ParserState* state = &parser->states[j];
      buffer_format_print(b, "%c %u %u %u %u\n", FORMAT_STATE, state->final, state->ss_cap, state->rr_cap, state->er_cap);
      for (unsigned k = 0; k < state->ss_cap; ++k) {
        struct Shift* shift = &state->ss_table[k];
        buffer_format_print(b, "%c %u %u\n", FORMAT_SHIFT, shift->symbol->index, shift->state);
      }

      for (unsigned k = 0; k < state->rr_cap; ++k) {
        struct Reduce* reduce = &state->rr_table[k];
        buffer_format_print(b, "%c %u %u\n", FORMAT_REDUCE, reduce->lhs->index, reduce->rs.index);
      }

      for (unsigned k = 0; k < state->er_cap; ++k) {
        Symbol* symbol = state->er_table[k];
        buffer_format_print(b, "%c %u\n", FORMAT_EPSILON, symbol->index);
      }
    }
  } while (0);

  return errors;
}

static struct Item* item_make(Parser* parser, Symbol* LHS, Symbol** RHS, unsigned index) {
  UNUSED(parser);
  struct Item* It = 0;
  MALLOC(struct Item, It);
  REF(It);
  It->lhs = LHS;
  It->rs.index = index;
  It->rhs_pos = It->rs.rules = RHS;
#if 0
  // TODO: add to hash table
  Number* num = numtab_lookup(parser->idx2rs, index, 1);
  assert(num);
  num->ptr = It;
  LOG_DEBUG("GONZO: parser %p: set rs %p for index %u", parser, It, index);
#endif
  return It;
}

static struct Item* item_clone(struct Item* It) {
  struct Item* clone = 0;
  MALLOC(struct Item, clone);
  *clone = *It;

  // careful with the clone's reference count!
  clone->ref_cnt = 0;
  REF(clone);

  return clone;
}

static void item_add(struct Items* Its, struct Item* It) {
  unsigned I = 0;
  for (I = 0; I < Its->item_cap; ++I) {
    int Diff = item_compare(Its->item_table[I], It);
    if (Diff == 0) return; // item exists
    if (Diff > 0) break;   // found first greater item
  }
  TABLE_CHECK_GROW(Its->item_table, Its->item_cap, 4, struct Item*);
  for (unsigned J = Its->item_cap++; J > I; --J) {
    // make room for item
    Its->item_table[J] = Its->item_table[J - 1];
  }
  // clone item into freed space
  Its->item_table[I] = item_clone(It);
}

static int item_compare(struct Item* l, struct Item* r) {
  int Diff = 0;

  if (l->lhs == 0 && r->lhs == 0) {
      Diff = 0;
  } else if (l->lhs == 0) {
      Diff = -1;
  } else if (r->lhs == 0) {
      Diff = +1;
  } else {
      Diff = l->lhs->index - r->lhs->index;
  }
  if (Diff != 0) return Diff;

  Diff = (l->rhs_pos - l->rs.rules) - (r->rhs_pos - r->rs.rules);
  if (Diff != 0) return Diff;

  Symbol** lptr = 0;
  Symbol** bptr = 0;
  for (lptr = l->rs.rules, bptr = r->rs.rules; *lptr && *bptr; ++lptr, ++bptr) {
    Diff = (*lptr)->index - (*bptr)->index;
    if (Diff != 0) break;
  }
  if (*lptr == 0 && *bptr == 0) {
    return 0;
  } else if (*lptr == 0) {
    return -1;
  } else if (*bptr == 0) {
    return +1;
  }

  return Diff;
}

static struct Items* items_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax) {
  LOG_DEBUG("items_get, Pre=%p, XTab=%p, Xs=%u, XMax=%u", Pre, *XTab, *Xs, *XMax);
  unsigned X = 0;
  for (X = 0; X < *Xs; ++X) {
    if (Pre == (*XTab)[X].Pre) break;
  }
  if (X >= *Xs) {
    if (*Xs >= *XMax) {
      *XMax += 8;
      REALLOC(struct Items, *XTab, *XMax);
    }
    X = (*Xs)++;
    (*XTab)[X].Pre = Pre;
    (*XTab)[X].item_cap = 0;
    (*XTab)[X].item_table = 0;
  }
  return &(*XTab)[X];
}

static void state_make(struct ParserState* state, unsigned char final, unsigned er_new, unsigned rr_new, unsigned ss_new) {
  state->final = final;
  state->er_cap = er_new;
  state->rr_cap = rr_new;
  state->ss_cap = ss_new;
  state->er_table = 0;
  state->rr_table = 0;
  state->ss_table = 0;
  MALLOC_N(Symbol*      , state->er_table, er_new);
  MALLOC_N(struct Reduce, state->rr_table, rr_new);
  MALLOC_N(struct Shift , state->ss_table, ss_new);
}

static int state_add(Parser* parser, struct Items** items_table, unsigned int Size, struct Item** List) {
  for (unsigned S = 0; S < parser->state_cap; ++S) {
    struct Items* IS = &(*items_table)[S];
    if (IS->item_cap != Size) continue;
    unsigned I = 0;
    for (I = 0; I < IS->item_cap; ++I) {
      if (item_compare(IS->item_table[I], List[I]) != 0) break;
    }
    if (I >= IS->item_cap) {
      for (I = 0; I < Size; ++I) {
        UNREF(List[I]);
      }
      FREE(List);
      return S;
    }
  }
  TABLE_CHECK_GROW(parser->states, parser->state_cap, 8, struct ParserState);
  TABLE_CHECK_GROW(*items_table, parser->state_cap, 8, struct Items);
  struct Items* IS = &(*items_table)[parser->state_cap];
  IS->Pre = 0;
  IS->item_cap = Size;
  IS->item_table = List;
  return parser->state_cap++;
}

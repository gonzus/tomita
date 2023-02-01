#include "log.h"
#include "mem.h"
#include "util.h"
#include "tomita.h"
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
  Symbol* Pre;               // TODO
  struct Item** item_table;  // Item table
  unsigned item_cap;         //   capacity of table
};

static void parser_reset(Parser* parser);
static struct Item* item_make(Symbol* LHS, Symbol** RHS, unsigned index);
static struct Items* items_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax);
static void item_add(struct Items* Q, struct Item* It);
static int item_compare(struct Item* A, struct Item* B);

static void state_make(struct State* S, unsigned char final, unsigned er_new, unsigned rr_new, unsigned ss_new);
static int state_add(Parser* parser, struct Items** items_table, unsigned int Size, struct Item** List);

Parser* parser_create(SymTab* symtab) {
  Parser* parser = 0;
  MALLOC(Parser, parser);
  buffer_build(&parser->source);
  parser->symtab = symtab;
  return parser;
}

void parser_destroy(Parser* parser) {
  parser_reset(parser);
  buffer_destroy(&parser->source);
  FREE(parser);
}

unsigned parser_build_from_grammar(Parser* parser, Grammar* grammar) {
  parser_reset(parser);
  parser->symtab = grammar->symtab;

  // Create initial state
  struct Items* items_table = 0;
  Symbol** StartR = 0;
  MALLOC_N(Symbol*, StartR, 2);
  StartR[0] = grammar->start;
  StartR[1] = 0;

  struct Item** Its = 0;
  MALLOC(struct Item*, Its);
  Its[0] = item_make(0, StartR, 666);

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
      } else {
        Symbol* Pre = *It->rhs_pos++;
        struct Items* IS = items_get(Pre, &XTab, &Xs, &XMax);
        if (IS->item_cap == 0) {
          if ((Qs + Pre->rs_cap) > QMax) {
            QMax = Qs + Pre->rs_cap;
            REALLOC(struct Item*, QBuf, QMax);
          }
          for (unsigned R = 0; R < Pre->rs_cap; ++R, ++Qs) {
            QBuf[Qs] = item_make(Pre, Pre->rs_table[R].rules, Pre->rs_table[R].index);
          }
        }
        item_add(IS, It);
        --It->rhs_pos;
      }
    }
    state_make(&parser->state_table[S], final, ERs, RRs, Xs);
    unsigned R = 0;
    unsigned E = 0;
    for (unsigned Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->rhs_pos != 0 || It->lhs == 0) continue;
      if (*It->rs.rules == 0) {
        parser->state_table[S].er_table[E++] = It->lhs;
      } else {
        struct Reduce* Rd = &parser->state_table[S].rr_table[R++];
        Rd->lhs = It->lhs;
        Rd->rs = It->rs;
      }
    }
    for (unsigned X = 0; X < Xs; ++X) {
      struct Shift* Sh = &parser->state_table[S].ss_table[X];
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
  unsigned conflict_sr = 0;
  unsigned conflict_rr = 0;
  for (unsigned S = 0; S < parser->state_cap; ++S) {
    printf("%d:\n", S);
    struct State* St = &parser->state_table[S];
    if (St->final) printf("\taccept\n");
    for (unsigned j = 0; j < St->er_cap; ++j) {
      Slice sn = St->er_table[j]->name;
      printf("\t[%.*s -> 1]\n", sn.len, sn.ptr);
    }

    for (unsigned j = 0; j < St->rr_cap; ++j) {
      struct Reduce* r = &St->rr_table[j];
      Slice rn = r->lhs->name;
      printf("\t[%.*s ->", rn.len, rn.ptr);
      for (Symbol** R = r->rs.rules; *R != 0; ++R) {
        Slice nn = (*R)->name;
        printf(" %.*s", nn.len, nn.ptr);
      }
      printf("]\n");
    }
    if (St->rr_cap > 1) {
      unsigned n = St->rr_cap - 1;
      printf("\t\t*** %u reduce/reduce conflict%s ***\n", n, n == 1 ? "" : "s");
      conflict_rr += n;
    }

    for (unsigned j = 0; j < St->ss_cap; ++j) {
      struct Shift* s = &St->ss_table[j];
      Slice sn = s->symbol->name;
      printf("\t%.*s => %d\n", sn.len, sn.ptr, s->state);
    }
    if (St->ss_cap > 0 && St->rr_cap > 0) {
      unsigned n = St->ss_cap;
      printf("\t\t*** %u shift/reduce conflict%s ***\n", n, n == 1 ? "" : "s");
      conflict_sr += n;
    }
  }
  printf("--------\n");
  printf("%u total shift/reduce conflicts\n", conflict_sr);
  printf("%u total reduce/reduce conflicts\n", conflict_rr);
}

unsigned parser_load_from_slice(Parser* parser, Slice source) {
  parser_reset(parser);
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
          TABLE_CHECK_GROW(parser->state_table, state_tot, 8, struct State);
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
        state_make(&parser->state_table[state_tot-1], final, er_cap, rr_cap, ss_cap);
        parser->state_table[state_tot-1].ss_cap = 0;
        parser->state_table[state_tot-1].rr_cap = 0;
        parser->state_table[state_tot-1].er_cap = 0;
        continue;
      }
      if (lead == FORMAT_SHIFT) {
        int t = parser->state_table[state_tot-1].ss_cap++;
        unsigned index = 0;
        unsigned state = 0;
        pos = next_number(line, pos, &index);
        pos = next_number(line, pos, &state);
        LOG_DEBUG("loaded shift: index=%u, state=%u", index, state);
        struct Shift* shift = &parser->state_table[state_tot-1].ss_table[t];
        shift->symbol = find_symbol_by_index(parser->symtab, index);
        shift->state = state;
        continue;
      }
      if (lead == FORMAT_REDUCE) {
        int t = parser->state_table[state_tot-1].rr_cap++;
        unsigned lhs_index = 0;
        unsigned rs_index = 0;
        pos = next_number(line, pos, &lhs_index);
        pos = next_number(line, pos, &rs_index);
        LOG_DEBUG("loaded reduce: lhs=%u rs=%u", lhs_index, rs_index);
        struct Reduce* reduce = &parser->state_table[state_tot-1].rr_table[t];
        Symbol* lhs = find_symbol_by_index(parser->symtab, lhs_index);
        assert(lhs);
        RuleSet* rs = find_ruleset_by_index(lhs, rs_index);
        assert(rs);
        reduce->lhs = lhs;
        reduce->rs = *rs;
        continue;
      }
      if (lead == FORMAT_EPSILON) {
        int t = parser->state_table[state_tot-1].er_cap++;
        unsigned index = 0;
        pos = next_number(line, pos, &index);
        LOG_DEBUG("loaded epsilon: index=%u", index);
        struct Symbol** epsilon = &parser->state_table[state_tot-1].er_table[t];
        Symbol* symbol = find_symbol_by_index(parser->symtab, index);
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
      struct State* state = &parser->state_table[j];
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

static struct Item* item_make(Symbol* LHS, Symbol** RHS, unsigned index) {
  struct Item* It = 0;
  MALLOC(struct Item, It);
  REF(It);
  It->lhs = LHS;
  It->rs.index = index;
  It->rhs_pos = It->rs.rules = RHS;
  return It;
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
  TABLE_CHECK_GROW(parser->state_table, parser->state_cap, 8, struct State);
  TABLE_CHECK_GROW(*items_table, parser->state_cap, 8, struct Items);
  (*items_table)[parser->state_cap].Pre = 0;
  (*items_table)[parser->state_cap].item_cap = Size;
  (*items_table)[parser->state_cap].item_table = List;
  return parser->state_cap++;
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

static struct Item* item_clone(struct Item* A) {
  struct Item* B = 0;
  MALLOC(struct Item, B);
  *B = *A;

  // careful with the clone's reference count!
  B->ref_cnt = 0;
  REF(B);

  return B;
}

static void item_add(struct Items* Q, struct Item* It) {
  unsigned I = 0;
  for (I = 0; I < Q->item_cap; ++I) {
    unsigned Diff = item_compare(Q->item_table[I], It);
    if (Diff == 0) return;
    if (Diff > 0) break;
  }
  TABLE_CHECK_GROW(Q->item_table, Q->item_cap, 4, struct Item*);
  for (unsigned J = Q->item_cap++; J > I; --J) {
    Q->item_table[J] = Q->item_table[J - 1];
  }
  Q->item_table[I] = item_clone(It);
}

static void state_make(struct State* S, unsigned char final, unsigned er_new, unsigned rr_new, unsigned ss_new) {
  S->final = final;
  S->er_cap = er_new;
  S->rr_cap = rr_new;
  S->ss_cap = ss_new;
  S->er_table = 0;
  S->rr_table = 0;
  S->ss_table = 0;
  MALLOC_N(Symbol*      , S->er_table, er_new);
  MALLOC_N(struct Reduce, S->rr_table, rr_new);
  MALLOC_N(struct Shift , S->ss_table, ss_new);
}

static int item_compare(struct Item* A, struct Item* B) {
  int Diff = 0;

  if (A->lhs == 0 && B->lhs == 0) {
      Diff = 0;
  } else if (A->lhs == 0) {
      Diff = -1;
  } else if (B->lhs == 0) {
      Diff = +1;
  } else {
      Diff = A->lhs->index - B->lhs->index;
  }
  if (Diff != 0) return Diff;

  Diff = (A->rhs_pos - A->rs.rules) - (B->rhs_pos - B->rs.rules);
  if (Diff != 0) return Diff;

  Symbol** AP = 0;
  Symbol** BP = 0;
  for (AP = A->rs.rules, BP = B->rs.rules; *AP && *BP; ++AP, ++BP) {
    Diff = (*AP)->index - (*BP)->index;
    if (Diff != 0) break;
  }
  if (*AP == 0 && *BP == 0) {
    return 0;
  } else if (*AP == 0) {
    return -1;
  } else if (*BP == 0) {
    return +1;
  }

  return Diff;
}

static void parser_reset(Parser* parser) {
  if (parser->state_table) {
    for (unsigned j = 0; j < parser->state_cap; ++j) {
      struct State* state = &parser->state_table[j];
      FREE(state->er_table);
      FREE(state->rr_table);
      FREE(state->ss_table);
    }
    FREE (parser->state_table);
  }
  buffer_clear(&parser->source);
  parser->state_table = 0;
  parser->state_cap = 0;
}

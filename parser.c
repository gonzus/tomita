#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "parser.h"

static void parser_build(Parser* parser);
static struct Item* item_make(Symbol* LHS, Symbol** RHS);
static struct Items* items_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax);
static void item_add(struct Items* Q, struct Item* It);
static int item_compare(struct Item* A, struct Item* B);

static void state_make(struct State* S, unsigned char final, unsigned er_new, unsigned rr_new, unsigned ss_new);
static int state_add(Parser* parser, unsigned int Size, struct Item** List);

Parser* parser_create(Grammar* grammar) {
  Parser* parser = 0;
  MALLOC(Parser, parser);
  parser->grammar = grammar;
  parser_build(parser);
  return parser;
}

void parser_destroy(Parser* parser) {
  if (parser->items_table) {
    for (unsigned j = 0; j < parser->table_cap; ++j) {
      struct Items* items = &parser->items_table[j];
      for (unsigned k = 0; k < items->item_cap; ++k) {
        UNREF(items->item_table[k]);
      }
      FREE(items->item_table);
    }
    FREE(parser->items_table);
  }
  if (parser->state_table) {
    for (unsigned j = 0; j < parser->table_cap; ++j) {
      struct State* state = &parser->state_table[j];
      FREE(state->er_table);
      FREE(state->rr_table);
      FREE(state->ss_table);
    }
    FREE (parser->state_table);
  }
  FREE(parser);
}

static void parser_build(Parser* parser) {
  parser->state_table = 0;
  parser->items_table = 0;
  parser->table_cap = 0;

  // Create initial state
  Symbol** StartR = 0;
  MALLOC_N(Symbol*, StartR, 2);
  StartR[0] = parser->grammar->start;
  StartR[1] = 0;

  struct Item** Its = 0;
  MALLOC(struct Item*, Its);
  Its[0] = item_make(0, StartR);

  state_add(parser, 1, Its);

  struct Items* XTab = 0;
  unsigned XMax = 0;

  struct Item** QBuf = 0;
  unsigned QMax = 0;
  unsigned Qs = 0;

  for (unsigned S = 0; S < parser->table_cap; ++S) {
    struct Items* QS = &parser->items_table[S];
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
        else if (*It->rhs_table == 0) {
          ++ERs;
        } else {
          ++RRs;
        }
      } else {
        Symbol* Pre = *It->rhs_pos++;
        struct Items* IS = items_get(Pre, &XTab, &Xs, &XMax);
        if (IS->item_cap == 0) {
          if ((Qs + Pre->rule_count) > QMax) {
            QMax = Qs + Pre->rule_count;
            REALLOC(struct Item*, QBuf, QMax);
          }
          for (unsigned R = 0; R < Pre->rule_count; ++R, ++Qs) {
            QBuf[Qs] = item_make(Pre, Pre->rules[R]);
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
      if (*It->rhs_table == 0) {
        parser->state_table[S].er_table[E++] = It->lhs;
      } else {
        struct Reduce* Rd = &parser->state_table[S].rr_table[R++];
        Rd->lhs = It->lhs;
        Rd->rhs = It->rhs_table;
      }
    }
    for (unsigned X = 0; X < Xs; ++X) {
      struct Shift* Sh = &parser->state_table[S].ss_table[X];
      Sh->symbol = XTab[X].Pre;
      Sh->state = state_add(parser, XTab[X].item_cap, XTab[X].item_table);
    }
    for (unsigned Q = 0; Q < Qs; ++Q) {
        UNREF(QBuf[Q]);
    }
  }
  FREE(XTab);
  FREE(QBuf);
  FREE(StartR);
}

void parser_show(Parser* parser) {
  unsigned conflict_sr = 0;
  unsigned conflict_rr = 0;
  for (unsigned S = 0; S < parser->table_cap; ++S) {
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
      for (Symbol** R = r->rhs; *R != 0; ++R) {
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

static struct Item* item_make(Symbol* LHS, Symbol** RHS) {
  struct Item* It = 0;
  MALLOC(struct Item, It);
  REF(It);
  It->lhs = LHS;
  It->rhs_pos = It->rhs_table = RHS;
  return It;
}

static int state_add(Parser* parser, unsigned int Size, struct Item** List) {
  for (unsigned S = 0; S < parser->table_cap; ++S) {
    struct Items* IS = &parser->items_table[S];
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
  TABLE_CHECK_GROW(parser->state_table, parser->table_cap, 8, struct State);
  TABLE_CHECK_GROW(parser->items_table, parser->table_cap, 8, struct Items);
  parser->items_table[parser->table_cap].Pre = 0;
  parser->items_table[parser->table_cap].item_cap = Size;
  parser->items_table[parser->table_cap].item_table = List;
  return parser->table_cap++;
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

  Diff = (A->rhs_pos - A->rhs_table) - (B->rhs_pos - B->rhs_table);
  if (Diff != 0) return Diff;

  Symbol** AP = 0;
  Symbol** BP = 0;
  for (AP = A->rhs_table, BP = B->rhs_table; *AP && *BP; ++AP, ++BP) {
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

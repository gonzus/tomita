#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "parser.h"

static void parser_build(Parser* parser);
static struct Item* item_make(Symbol* LHS, Symbol** RHS);
static struct Items* item_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax);
static void item_add(struct Items* Q, struct Item* It);
static int item_compare(struct Item* A, struct Item* B);

static void state_make(struct State* S, unsigned char Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss);
static int state_add(Parser* parser, unsigned int Size, struct Item** List);

Parser* parser_create(Grammar* grammar) {
  Parser* parser = 0;
  MALLOC(Parser, parser);
  parser->grammar = grammar;
  parser_build(parser);
  return parser;
}

void parser_destroy(Parser* parser) {
  if (parser->STab) {
    for (unsigned j = 0; j < parser->Ss; ++j) {
      struct Items* STab = &parser->STab[j];
      for (unsigned k = 0; k < STab->Size; ++k) {
        UNREF(STab->List[k]);
      }
      FREE(STab->List);
    }
    FREE(parser->STab);
  }
  if (parser->SList) {
    for (unsigned j = 0; j < parser->Ss; ++j) {
      struct State* SList = &parser->SList[j];
      FREE(SList->EList);
      FREE(SList->RList);
      FREE(SList->SList);
    }
    FREE (parser->SList);
  }
  FREE(parser);
}

static void parser_build(Parser* parser) {
  parser->SList = 0;
  parser->STab = 0;
  parser->Ss = 0;

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

  for (unsigned S = 0; S < parser->Ss; ++S) {
    struct Items* QS = &parser->STab[S];
    if (QS->Size > QMax) {
      QMax = QS->Size;
      REALLOC(struct Item*, QBuf, QMax);
    }
    for (Qs = 0; Qs < QS->Size; ++Qs) {
      QBuf[Qs] = REF(QS->List[Qs]);
    }
    unsigned ERs = 0;
    unsigned RRs = 0;
    unsigned char Final = 0;
    unsigned Xs = 0;
    for (unsigned Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->Pos == 0) {
        if (It->LHS == 0) {
          ++Final;
        }
        else if (*It->RHS == 0) {
          ++ERs;
        } else {
          ++RRs;
        }
      } else {
        Symbol* Pre = *It->Pos++;
        struct Items* IS = item_get(Pre, &XTab, &Xs, &XMax);
        if (IS->Size == 0) {
          if ((Qs + Pre->rule_count) > QMax) {
            QMax = Qs + Pre->rule_count;
            REALLOC(struct Item*, QBuf, QMax);
          }
          for (unsigned R = 0; R < Pre->rule_count; ++R, ++Qs) {
            QBuf[Qs] = item_make(Pre, Pre->rules[R]);
          }
        }
        item_add(IS, It);
        --It->Pos;
      }
    }
    state_make(&parser->SList[S], Final, ERs, RRs, Xs);
    unsigned R = 0;
    unsigned E = 0;
    for (unsigned Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->Pos != 0 || It->LHS == 0) continue;
      if (*It->RHS == 0) {
        parser->SList[S].EList[E++] = It->LHS;
      } else {
        struct Reduce* Rd = &parser->SList[S].RList[R++];
        Rd->LHS = It->LHS;
        Rd->RHS = It->RHS;
      }
    }
    for (unsigned X = 0; X < Xs; ++X) {
      struct Shift* Sh = &parser->SList[S].SList[X];
      Sh->X = XTab[X].Pre;
      Sh->Q = state_add(parser, XTab[X].Size, XTab[X].List);
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
  for (unsigned S = 0; S < parser->Ss; ++S) {
    printf("%d:\n", S);
    struct State* St = &parser->SList[S];
    if (St->Final) printf("\taccept\n");
    for (unsigned j = 0; j < St->Es; ++j) {
      Slice sn = St->EList[j]->name;
      printf("\t[%.*s -> 1]\n", sn.len, sn.ptr);
    }

    for (unsigned j = 0; j < St->Rs; ++j) {
      struct Reduce* r = &St->RList[j];
      Slice rn = r->LHS->name;
      printf("\t[%.*s ->", rn.len, rn.ptr);
      for (Symbol** R = r->RHS; *R != 0; ++R) {
        Slice nn = (*R)->name;
        printf(" %.*s", nn.len, nn.ptr);
      }
      printf("]\n");
    }
    if (St->Rs > 1) {
      unsigned n = St->Rs - 1;
      printf("\t\t*** %u reduce/reduce conflict%s ***\n", n, n == 1 ? "" : "s");
      conflict_rr += n;
    }

    for (unsigned j = 0; j < St->Ss; ++j) {
      struct Shift* s = &St->SList[j];
      Slice sn = s->X->name;
      printf("\t%.*s => %d\n", sn.len, sn.ptr, s->Q);
    }
    if (St->Ss > 0 && St->Rs > 0) {
      unsigned n = St->Ss;
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
  It->LHS = LHS;
  It->Pos = It->RHS = RHS;
  return It;
}

static int state_add(Parser* parser, unsigned int Size, struct Item** List) {
  for (unsigned S = 0; S < parser->Ss; ++S) {
    struct Items* IS = &parser->STab[S];
    if (IS->Size != Size) continue;
    unsigned I = 0;
    for (I = 0; I < IS->Size; ++I) {
      if (item_compare(IS->List[I], List[I]) != 0) break;
    }
    if (I >= IS->Size) {
      for (I = 0; I < Size; ++I) {
        UNREF(List[I]);
      }
      FREE(List);
      return S;
    }
  }
  if ((parser->Ss & 7) == 0) {
    REALLOC(struct Items, parser->STab, parser->Ss + 8);
    REALLOC(struct State, parser->SList, parser->Ss + 8);
  }
  parser->STab[parser->Ss].Pre = 0;
  parser->STab[parser->Ss].Size = Size;
  parser->STab[parser->Ss].List = List;
  return parser->Ss++;
}

static struct Items* item_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax) {
  LOG_DEBUG("item_get, Pre=%p, XTab=%p, Xs=%u, XMax=%u", Pre, *XTab, *Xs, *XMax);
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
    (*XTab)[X].Size = 0;
    (*XTab)[X].List = 0;
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
  for (I = 0; I < Q->Size; ++I) {
    unsigned Diff = item_compare(Q->List[I], It);
    if (Diff == 0) return;
    if (Diff > 0) break;
  }
  if ((Q->Size & 3) == 0) {
    REALLOC(struct Item*, Q->List, Q->Size + 4);
  }
  for (unsigned J = Q->Size++; J > I; --J) {
    Q->List[J] = Q->List[J - 1];
  }
  Q->List[I] = item_clone(It);
}

static void state_make(struct State* S, unsigned char Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss) {
  S->Final = Final;
  S->Es = new_Es;
  S->Rs = new_Rs;
  S->Ss = new_Ss;
  S->EList = 0;
  S->RList = 0;
  S->SList = 0;
  MALLOC_N(Symbol*      , S->EList, new_Es);
  MALLOC_N(struct Reduce, S->RList, new_Rs);
  MALLOC_N(struct Shift , S->SList, new_Ss);
}

static int item_compare(struct Item* A, struct Item* B) {
  int Diff = 0;

  if (A->LHS == 0 && B->LHS == 0) {
      Diff = 0;
  } else if (A->LHS == 0) {
      Diff = -1;
  } else if (B->LHS == 0) {
      Diff = +1;
  } else {
      Diff = A->LHS->index - B->LHS->index;
  }
  if (Diff != 0) return Diff;

  Diff = (A->Pos - A->RHS) - (B->Pos - B->RHS);
  if (Diff != 0) return Diff;

  Symbol** AP = 0;
  Symbol** BP = 0;
  for (AP = A->RHS, BP = B->RHS; *AP && *BP; ++AP, ++BP) {
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

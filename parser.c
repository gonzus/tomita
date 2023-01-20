#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "mem.h"
#include "parser.h"

static struct Item* item_make(Symbol* LHS, Rule RHS);
static struct Items* item_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax);
static void item_add(struct Items* Q, struct Item* It);
static int item_compare(struct Item* A, struct Item* B);

static void state_make(struct State* S, unsigned char Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss);
static int state_add(Parser* parser, unsigned int Size, struct Item** List);

Parser* parser_create(void) {
  Parser* parser = (Parser*) malloc(sizeof(Parser));
  memset(parser, 0, sizeof(Parser));
  return parser;
}

void parser_destroy(Parser* parser) {
  free(parser);
}

void parser_build(Parser* parser, Grammar* grammar) {
  Rule StartR = Allocate(2 * sizeof(Symbol*));
  StartR[0] = grammar->start;
  StartR[1] = 0;

  struct Item** Its = Allocate(sizeof(struct Item*));
  Its[0] = item_make(0, StartR);

  parser->SList = 0;
  parser->STab = 0;
  parser->Ss = 0;
  state_add(parser, 1, Its);

  struct Items* XTab = 0;
  unsigned XMax = 0;

  struct Item** QBuf = 0;
  unsigned QMax = 0;

  for (unsigned S = 0; S < parser->Ss; ++S) {
    unsigned ERs;
    unsigned RRs;
    unsigned E;
    unsigned Q;
    unsigned R;
    unsigned Qs;
    unsigned Xs = 0;
    unsigned char Final;
    struct Items* QS = &parser->STab[S];
    if (QS->Size > QMax) {
      QMax = QS->Size;
      QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
    }
    for (Qs = 0; Qs < QS->Size; Qs++) {
      QBuf[Qs] = QS->List[Qs];
    }
    for (ERs = RRs = 0, Final = 0, Xs = 0, Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->Pos == 0) {
        if (It->LHS == 0) {
          Final++;
        }
        else if (*It->RHS == 0) {
          ERs++;
        } else {
          RRs++;
        }
      } else {
        Symbol* Pre = *It->Pos++;
        struct Items* IS = item_get(Pre, &XTab, &Xs, &XMax);
        if (IS->Size == 0) {
          if ((Qs + Pre->rule_count) > QMax) {
            QMax = Qs + Pre->rule_count;
            QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
          }
          for (R = 0; R < Pre->rule_count; ++R, ++Qs) {
            QBuf[Qs] = item_make(Pre, Pre->rules[R]);
          }
        }
        item_add(IS, It);
        --It->Pos;
      }
    }
    state_make(&parser->SList[S], Final, ERs, RRs, Xs);
    for (E = R = 0, Q = 0; Q < Qs; ++Q) {
      struct Item* It = QBuf[Q];
      if (*It->Pos != 0 || It->LHS == 0) continue;
      if (*It->RHS == 0) {
        parser->SList[S].EList[E++] = It->LHS;
      } else {
        struct Reduce* Rd = &parser->SList[S].RList[R++];
        Rd->LHS = It->LHS, Rd->RHS = It->RHS;
      }
    }
    for (unsigned X = 0; X < Xs; X++) {
      struct Shift* Sh = &parser->SList[S].SList[X];
      Sh->X = XTab[X].Pre, Sh->Q = state_add(parser, XTab[X].Size, XTab[X].List);
    }
  }
  free(XTab);
  free(QBuf);
}

void parser_show(Parser* parser) {
  for (unsigned S = 0; S < parser->Ss; S++) {
    printf("%d:\n", S);
    struct State* St = &parser->SList[S];
    if (St->Final) printf("\taccept\n");
    for (unsigned I = 0; I < St->Es; ++I) {
      Slice sn = St->EList[I]->name;
      printf("\t[%.*s -> 1]\n", sn.len, sn.ptr);
    }
    for (unsigned I = 0; I < St->Rs; ++I) {
      Slice rn = St->RList[I].LHS->name;
      printf("\t[%.*s ->", rn.len, rn.ptr);
      for (Rule R = St->RList[I].RHS; *R != 0; ++R) {
        Slice nn = (*R)->name;
        printf(" %.*s", nn.len, nn.ptr);
      }
      printf("]\n");
    }
    for (unsigned I = 0; I < St->Ss; ++I) {
      Slice xn = St->SList[I].X->name;
      printf("\t%.*s => %d\n", xn.len, xn.ptr, St->SList[I].Q);
    }
  }
}

static struct Item* item_make(Symbol* LHS, Rule RHS) {
  struct Item* It = Allocate(sizeof *It);
  It->LHS = LHS;
  It->Pos = It->RHS = RHS;
  return It;
}

static int state_add(Parser* parser, unsigned int Size, struct Item** List) {
  unsigned I;
  unsigned S;
  for (S = 0; S < parser->Ss; S++) {
    struct Items* IS = &parser->STab[S];
    if (IS->Size != Size) continue;
    for (I = 0; I < IS->Size; I++) {
      if (item_compare(IS->List[I], List[I]) != 0) break;
    }
    if (I >= IS->Size) {
      free(List);
      return S;
    }
  }
  if ((parser->Ss & 7) == 0) {
    parser->STab = Reallocate(parser->STab, (parser->Ss + 8) * sizeof *parser->STab);
    parser->SList = Reallocate(parser->SList, (parser->Ss + 8) * sizeof *parser->SList);
  }
  parser->STab[parser->Ss].Pre = 0;
  parser->STab[parser->Ss].Size = Size;
  parser->STab[parser->Ss].List = List;
  return parser->Ss++;
}

static struct Items* item_get(Symbol* Pre, struct Items** XTab, unsigned* Xs, unsigned* XMax) {
  LOG_DEBUG("item_get, Pre=%p, XTab=%p, Xs=%u, XMax=%u", Pre, *XTab, *Xs, *XMax);
  unsigned X;
  for (X = 0; X < *Xs; ++X) {
    if (Pre == (*XTab)[X].Pre) break;
  }
  if (X >= *Xs) {
    if (*Xs >= *XMax) {
      *XMax += 8;
      *XTab = Reallocate(*XTab, *XMax * sizeof **XTab);
    }
    X = (*Xs)++;
    (*XTab)[X].Pre = Pre;
    (*XTab)[X].Size = 0;
    (*XTab)[X].List = 0;
  }
  return &(*XTab)[X];
}

static struct Item* item_copy(struct Item* A) {
  struct Item* B = Allocate(sizeof *B);
  *B = *A;
  return B;
}

static void item_add(struct Items* Q, struct Item* It) {
  unsigned I;
  for (I = 0; I < Q->Size; I++) {
    unsigned Diff = item_compare(Q->List[I], It);
    if (Diff == 0) return;
    if (Diff > 0) break;
  }
  if ((Q->Size&3) == 0) {
    Q->List = Reallocate(Q->List, (Q->Size + 4) * sizeof(struct Item*));
  }
  for (unsigned J = Q->Size++; J > I; J--) {
    Q->List[J] = Q->List[J - 1];
  }
  Q->List[I] = item_copy(It);
}

static void state_make(struct State* S, unsigned char Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss) {
  S->Final = Final;
  S->Es = new_Es, S->EList = new_Es == 0 ? 0 : Allocate(new_Es * sizeof(Symbol));
  S->Rs = new_Rs, S->RList = new_Rs == 0 ? 0 : Allocate(new_Rs * sizeof(struct Reduce));
  S->Ss = new_Ss, S->SList = new_Ss == 0 ? 0 : Allocate(new_Ss * sizeof(struct Shift));
}

static int item_compare(struct Item* A, struct Item* B) {
  int Diff =
    A->LHS == 0 ? (B->LHS == 0 ? 0 : -1):
    B->LHS == 0 ? +1 : A->LHS->index - B->LHS->index;
  if (Diff != 0) return Diff;

  Diff = (A->Pos - A->RHS) - (B->Pos - B->RHS);
  if (Diff != 0) return Diff;

  Symbol** AP = 0;
  Symbol** BP = 0;
  for (AP = A->RHS, BP = B->RHS; *AP != 0 && *BP != 0; AP++, BP++) {
    Diff = (*AP)->index - (*BP)->index;
    if (Diff != 0) break;
  }
  return *AP == 0 ? (*BP == 0 ? 0 : -1) : *BP == 0 ? +1 : Diff;
}

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "mem.h"
#include "forest.h"

struct Subnode {
  unsigned Size;
  unsigned Cur;
  unsigned Links;
  struct Subnode* Next;
};

struct Node {
  Symbol* Sym;
  unsigned Start;
  unsigned Size;
  unsigned Subs;
  struct Subnode** Sub;
};

struct ZNode {
  unsigned Val;
  unsigned Size;
  unsigned* List;
};

struct Path {
  struct ZNode* Z;
  struct Subnode* P;
};

struct Vertex {
  struct State* Val;
  unsigned Start;
  unsigned Size;
  struct ZNode** List;
};

struct RRed {
  struct ZNode* Z;
  Symbol* LHS;
  Rule RHS;
};

struct ERed {
  unsigned W;
  Symbol* LHS;
};

static void forest_prepare(Forest* forest);
static void forest_cleanup(Forest* forest);
static void FreeSub(struct Subnode* A);
static unsigned AddQ(Forest* forest, struct State* S);
static void Reduce1(Forest* forest, struct ZNode* Z, Symbol* L, Rule R);
static void AddN(Forest* forest, unsigned N, unsigned W);
static unsigned AddSub(Forest* forest, Symbol* L, struct Subnode* P);
static void AddRed(Forest* forest, struct ZNode* Z, Symbol* LHS, Rule RHS);
static void AddERed(Forest* forest, unsigned W, Symbol* LHS);
static void AddLink(Forest* forest, struct ZNode* Z, struct Subnode* P);
static struct State* Next(Forest* forest, struct State* Q, Symbol* Sym);
static int EqualS(struct Subnode* A, struct Subnode* B);
static unsigned next_symbol(Forest* forest, Slice text, unsigned pos, Symbol** sym);

Forest* forest_create(Parser* parser) {
  Forest* forest = (Forest*) malloc(sizeof(Forest));
  memset(forest, 0, sizeof(Forest));
  forest->parser = parser;
  return forest;
}

void forest_destroy(Forest* forest) {
  forest_cleanup(forest);
  free(forest);
}

static void forest_prepare(Forest* forest) {
  if (forest->prepared) return;
  forest->prepared = 1;
  LOG_INFO("preparing forest");

  forest->NodeTab = 0;
  forest->NodeE = 0;
  forest->Position = 0;
  forest->VertTab = 0;
  forest->VertE = 0;
  forest->PathTab = 0;
  forest->PathE = 0;
  forest->REDS = 0;
  forest->RP = forest->RE = 0;
  forest->EREDS = 0;
  forest->EP = forest->EE = 0;
  forest->NodeP = forest->NodeE;
  forest->VertP = forest->VertE;
}

static void forest_cleanup(Forest* forest) {
  if (!forest->prepared) return;
  forest->prepared = 0;
  LOG_INFO("cleaning up forest");

  unsigned int N, S, W, Z; struct Vertex* V; struct ZNode* ZN; struct Node* Nd;
  for (N = 0; N < forest->NodeE; N++) {
    Nd = &forest->NodeTab[N];
    for (S = 0; S < Nd->Subs; S++) {
      FreeSub(Nd->Sub[S]);
    }
    free(Nd->Sub);
  }
  free(forest->NodeTab);
  forest->NodeE = forest->NodeP = 0;
  for (W = 0; W < forest->VertE; W++) {
    V = &forest->VertTab[W];
    for (Z = 0; Z < V->Size; Z++) {
      ZN = V->List[Z];
      free(ZN->List);
      free(ZN);
    }
    free(V->List);
  }
  free(forest->VertTab);
  forest->VertE = forest->VertP = 0;
  free(forest->REDS);
  forest->RE = forest->RP = 0;
  free(forest->EREDS);
  forest->EE = forest->EP = 0;
}

struct Node* forest_parse(Forest* forest, Slice text) {
  unsigned pos = 0;
  Symbol* Word;
  unsigned C;
  unsigned VP;
  unsigned N;
  unsigned W;
  struct Subnode* P;
  forest_cleanup(forest);
  forest_prepare(forest);
  AddQ(forest, &forest->parser->SList[0]);
  while (1) {
    /* REDUCE */
    while (forest->EP < forest->EE || forest->RP < forest->RE) {
      for (; forest->RP < forest->RE; forest->RP++) {
        Reduce1(forest, forest->REDS[forest->RP].Z, forest->REDS[forest->RP].LHS, forest->REDS[forest->RP].RHS);
      }
      for (; forest->EP < forest->EE; forest->EP++) {
        AddN(forest, AddSub(forest, forest->EREDS[forest->EP].LHS, 0), forest->EREDS[forest->EP].W);
      }
    }
    /* SHIFT */
    pos = next_symbol(forest, text, pos, &Word);
    if (Word == 0) break;
    // printf(" %.*s", Word->name.len, Word->name.ptr);
    P = Allocate(sizeof(struct Subnode));
    P->Size = 1;
    P->Cur = AddSub(forest, Word, 0);
    P->Next = 0;
    P->Links = 0;
    VP = forest->VertP, forest->VertP = forest->VertE; forest->NodeP = forest->NodeE;
    if (Word->rule_count == 0) { /* Treat the word as a new word. */
      Symbol* S;
      for (S = forest->parser->grammar->first; S != 0; S = S->tail) {
        if (S->rule_count > 0) continue;
        N = AddSub(forest, S, P);
        for (W = VP; W < forest->VertP; W++) {
          AddN(forest, N, W);
        }
      }
    } else {
      for (C = 0; C < Word->rule_count; C++) {
        N = AddSub(forest, *Word->rules[C], P);
        for (W = VP; W < forest->VertP; W++) {
          AddN(forest, N, W);
        }
      }
    }
  }
  /* ACCEPT */
  // printf("\n");
  for (W = forest->VertP; W < forest->VertE; W++) {
    struct Vertex* V = &forest->VertTab[W];
    if (V->Val->Final) {
      return &forest->NodeTab[V->List[0]->Val];
    }
  }
  return 0;
}

static void FreeSub(struct Subnode* A) {
  while (A != 0) {
    if (--A->Links > 0) break;
    struct Subnode* Next = A->Next;
    free(A);
    A = Next;
  }
}

static unsigned AddQ(Forest* forest, struct State* S) {
  unsigned V;
  struct Vertex* W;
  unsigned int E;
  for (V = forest->VertP; V < forest->VertE; V++) {
    W = &forest->VertTab[V];
    if (W->Val == S) return V;
  }
  if ((forest->VertE & 7) == 0) {
    forest->VertTab = Reallocate(forest->VertTab, (forest->VertE + 8)*sizeof(struct Vertex));
  }
  W = &forest->VertTab[forest->VertE];
  W->Val = S, W->Start = forest->Position, W->Size = 0, W->List = 0;
  for (E = 0; E < S->Es; E++) {
    AddERed(forest, forest->VertE, S->EList[E]);
  }
  return forest->VertE++;
}

static void Reduce1(Forest* forest, struct ZNode* Z, Symbol* L, Rule R) {
  unsigned PE, X; struct Path* PP; struct Subnode* P; struct Vertex* V;
  unsigned PathP;
  forest->PathTab = 0;
  forest->PathE = PathP = 0;
  AddLink(forest, Z, 0);
  for (R++; *R != 0; R++) {
    for (PE = forest->PathE; PathP < PE; PathP++) {
      PP = &forest->PathTab[PathP];
      Z = PP->Z;
      P = PP->P;
      for (unsigned W = 0; W < Z->Size; W++) {
        V = &forest->VertTab[Z->List[W]];
        for (X = 0; X < V->Size; X++) {
          AddLink(forest, V->List[X], P);
        }
      }
    }
  }
  for (; PathP < forest->PathE; PathP++) {
    unsigned N;
    PP = &forest->PathTab[PathP];
    Z = PP->Z;
    P = PP->P;
    N = AddSub(forest, L, P);
    for (unsigned W = 0; W < Z->Size; W++) {
      AddN(forest, N, Z->List[W]);
    }
  }
  free(forest->PathTab);
  PathP = forest->PathE = 0;
}

static void AddN(Forest* forest, unsigned N, unsigned W) {
  struct Node* Nd = &forest->NodeTab[N];
  struct State* S = Next(forest, forest->VertTab[W].Val, Nd->Sym);
  struct Vertex* W1;
  unsigned Z;
  struct ZNode* Z1;
  unsigned int I;
  if (S == 0) return;
#if 0
  // on my M1 laptop, this does not work...
  W1 = &forest->VertTab[AddQ(forest, S)];
#else
  // ... but this does -- compiler bug?
  // fix by gonzo
  unsigned pos = AddQ(forest, S);
  W1 = &forest->VertTab[pos];
#endif
  for (Z = 0; Z < W1->Size; Z++) {
    Z1 = W1->List[Z];
    if (Z1->Val == N) break;
  }
  if (Z >= W1->Size) {
    struct Reduce* Rd;
    unsigned int R;
    if ((W1->Size & 3) == 0)
      W1->List = Reallocate(W1->List, (W1->Size + 4) * sizeof(struct ZNode*));
    Z = W1->Size++;
    W1->List[Z] = Z1 = Allocate(sizeof *Z1);
    Z1->Val = N;
    Z1->Size = 0;
    Z1->List = 0;
    for (R = 0; R < S->Rs; R++) {
      Rd = &S->RList[R], AddRed(forest, Z1, Rd->LHS, Rd->RHS);
    }
  }
  for (I = 0; I < Z1->Size; I++) {
    if (Z1->List[I] == W) break;
  }
  if (I >= Z1->Size) {
    if ((Z1->Size&3) == 0) {
      Z1->List = Reallocate(Z1->List, (Z1->Size + 4)*sizeof *Z1->List);
    }
    I = Z1->Size++;
    Z1->List[I] = W;
  }
}

static unsigned AddSub(Forest* forest, Symbol* L, struct Subnode* P) {
  unsigned N, S, Size; struct Node* Nd;
  Size = L->literal? 1: (P == 0)? 0: P->Size;
  for (N = forest->NodeP; N < forest->NodeE; N++) {
    Nd = &forest->NodeTab[N];
    if (Nd->Sym == L && Nd->Size == Size) break;
  }
  if (N >= forest->NodeE) {
    if ((forest->NodeE & 7) == 0) {
      forest->NodeTab = Reallocate(forest->NodeTab, (forest->NodeE + 8) * sizeof(struct Node));
    }
    N = forest->NodeE++;
    Nd = &forest->NodeTab[N];
    Nd->Sym = L;
    Nd->Size = Size;
    Nd->Start = L->literal ? forest->Position - 1
              : (P == 0)   ? forest->Position
              : forest->NodeTab[P->Cur].Start;
    Nd->Subs = 0;
    Nd->Sub = 0;
  }
  if (!L->literal) {
    for (S = 0; S < Nd->Subs; S++) {
      if (EqualS(Nd->Sub[S], P)) break;
    }
    if (S >= Nd->Subs) {
      if ((Nd->Subs & 3) == 0) {
        Nd->Sub = Reallocate(Nd->Sub, (Nd->Subs + 4) * sizeof(struct Subnode*));
      }
      // we are adding a reference to this Subnode, increment its reference count
      // fix by gonzo
      ++P->Links;
      Nd->Sub[Nd->Subs++] = P;
    } else {
      FreeSub(P);
    }
  }
  return N;
}

static void AddRed(Forest* forest, struct ZNode* Z, Symbol* LHS, Rule RHS) {
  if ((forest->RE&7) == 0) {
    forest->REDS = Reallocate(forest->REDS, (forest->RE + 8)*sizeof *forest->REDS);
  }
  forest->REDS[forest->RE].Z = Z;
  forest->REDS[forest->RE].LHS = LHS;
  forest->REDS[forest->RE].RHS = RHS;
  forest->RE++;
}

static void AddERed(Forest* forest, unsigned W, Symbol* LHS) {
  if ((forest->EE & 7) == 0) {
    forest->EREDS = Reallocate(forest->EREDS, (forest->EE + 8)*sizeof *forest->EREDS);
  }
  forest->EREDS[forest->EE].W = W;
  forest->EREDS[forest->EE].LHS = LHS;
  forest->EE++;
}

static void AddLink(Forest* forest, struct ZNode* Z, struct Subnode* P) {
  struct Path* PP;
  unsigned N = Z->Val;
  struct Node* Nd = &forest->NodeTab[N];
  struct Subnode* NewP = Allocate(sizeof *NewP);
  NewP->Size = Nd->Size; if (P != 0) NewP->Size += P->Size, P->Links++;
  NewP->Cur = N, NewP->Next = P, NewP->Links = 0;
  P = NewP;
  if ((forest->PathE & 7) == 0) {
    forest->PathTab = Reallocate(forest->PathTab, (forest->PathE + 8) * sizeof *forest->PathTab);
  }
  PP = &forest->PathTab[forest->PathE++];
  PP->Z = Z;
  PP->P = P;
}

static struct State* Next(Forest* forest, struct State* Q, Symbol* Sym) {
  unsigned int S; struct Shift* Sh;
  for (S = 0; S < Q->Ss; S++) {
    Sh = &Q->SList[S];
    if (Sh->X == Sym) return &forest->parser->SList[Sh->Q];
  }
  return 0;
}

static int EqualS(struct Subnode* A, struct Subnode* B) {
  for (; A != 0 && B != 0; A = A->Next, B = B->Next)
    if (A->Size != B->Size || A->Cur != B->Cur) return 0;
  return A == B;
}

static unsigned next_symbol(Forest* forest, Slice text, unsigned pos, Symbol** sym) {
  *sym = 0;
  do {
    // skip whitespace
    while (pos < text.len && (text.ptr[pos] == ' ' || text.ptr[pos] == '\t')) ++pos;

    // ran out? done
    if (pos >= text.len) break;

    // work interactively
    if (text.ptr[pos] == '\n') {
      ++pos;
      break;
    }

    // gather non-space characters as symbol name
    unsigned beg = pos;
    while (pos < text.len && !isspace(text.ptr[pos])) ++pos;
    Slice name = slice_from_memory(text.ptr + beg, pos - beg);
    symtab_lookup(forest->parser->grammar->symtab, name, 1, sym);
    ++forest->Position;
  } while (0);

  LOG_DEBUG("symbol %p [%.*s]", *sym, *sym ? (*sym)->name.len : 0, *sym ? (*sym)->name.ptr : 0);
  return pos;
}

void forest_show_node(Forest* forest, struct Node* node) {
  (void) forest;
  Slice name = node->Sym->name;
  if (node->Sym->literal) {
    printf(" \"%.*s\"", name.len, name.ptr);
  }
  else {
    printf(" %.*s_%d_%d", name.len, name.ptr, node->Start, node->Start + node->Size);
  }
}

void forest_show(Forest* forest) {
  unsigned N;
  unsigned S;
  struct Node* Nd;
  struct Subnode* P;
  for (N = 0; N < forest->NodeE; N++) {
    Nd = &forest->NodeTab[N];
    if (Nd->Sym->literal) continue;
    forest_show_node(forest, Nd);
    if (Nd->Subs > 0) {
      P = Nd->Sub[0];
      if (forest->NodeTab[P->Cur].Sym->literal) {
        putchar(':');
      } else {
        printf(" =");
      }
    }
    for (S = 0; S < Nd->Subs; S++) {
      if (S > 0) {
        printf(" |");
      }
      for (P = Nd->Sub[S]; P != 0; P = P->Next) {
        struct Node* node = &forest->NodeTab[P->Cur];
        forest_show_node(forest, node);
      }
    }
    printf(".\n");
  }
}

static void show_vertex(Forest* forest, unsigned W) {
  struct Vertex* V = &forest->VertTab[W];
  printf(" v_%d_%ld", V->Start, V->Val - forest->parser->SList);
}

void forest_show_stack(Forest* forest) {
  unsigned int W, W1, Z;
  struct Vertex* V;
  struct ZNode* N;
  for (W = 0; W < forest->VertE; W++) {
    V = &forest->VertTab[W];
    show_vertex(forest, W);
    if (V->Size > 0) {
      printf(" <=\t");
    } else {
      putchar('\n');
    }
    for (Z = 0; Z < V->Size; Z++) {
      if (Z > 0) {
        printf("\t\t");
      }
      N = V->List[Z];
      struct Node* node = &forest->NodeTab[N->Val];
      printf(" ["), forest_show_node(forest, node), printf(" ] <=");
      for (W1 = 0; W1 < N->Size; W1++) {
        show_vertex(forest, N->List[W1]);
      }
      putchar('\n');
    }
  }
}

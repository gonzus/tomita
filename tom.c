#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"
#include "lex.h"
#include "sym.h"

/* An implementation of the Tomita Parsing Algorithm, using LR(0) parsing.
   Suggested modifications:
    + Generalize the LR(k) parsing automaton to a form suitable for handling
      grammars with rules of the form:
                     Non-Terminal = Regular-Expresion

    + Extend the LR(0) parser to LALR(1).

    + Add in attributes and error-handling.
 */

/* LR(0) PARSER GENERATOR */
/* Input format derived from the syntax:
   Grammar = Rule+.
   Rule = "*" ID "." |
          ID "." |
          ID "=" ID* ("|" ID*)* "." |
          ID ":" ID+ ".".
 */

static int ERRORS = 0;


/* TOMITA PARSER */
unsigned Position;
Symbol GetC(void) {
  int Ch; Symbol Sym;
  do Ch = GET(); while (Ch == ' ' || Ch == '\t');
  if (Ch == '\n' || Ch == EOF) return 0;
  for (LastW = ChP; Ch != EOF && !isspace(Ch); ChP++) {
    if (ChP - ChArr == MAX_CHAR)
      printf("Out of character space.\n"), exit(1);
    *ChP = Ch, Ch = GET();
  }
  if (Ch != EOF) UNGET(Ch);
  if (ChP - ChArr == MAX_CHAR) printf("Out of character space.\n"), exit(1);
  *ChP++ = '\0';
  Position++;
  return Sym = LookUp(LastW, 1);
}

struct Node { Symbol Sym; unsigned Start, Size, Subs; struct Subnode **Sub; };
struct Subnode { unsigned Size, Cur, Links; struct Subnode* Next; };
struct Node* NodeTab; unsigned NodeE, NodeP;

void SHOW_NODE(unsigned N) {
  struct Node* Nd = &NodeTab[N];
  if (Nd->Sym->Literal) printf(" \"%s\"", Nd->Sym->Name);
  else printf(" %s_%d_%d", Nd->Sym->Name, Nd->Start, Nd->Start + Nd->Size);
}

void SHOW_FOREST(void) {
  unsigned int N, S; struct Node* Nd; struct Subnode* P;
  for (N = 0; N < NodeE; N++) {
    Nd = &NodeTab[N];
    if (Nd->Sym->Literal) continue;
    SHOW_NODE(N);
    if (Nd->Subs > 0) {
      P = Nd->Sub[0];
      if (NodeTab[P->Cur].Sym->Literal) putchar(':'); else printf(" =");
    }
    for (S = 0; S < Nd->Subs; S++) {
      if (S > 0) printf(" |");
      for (P = Nd->Sub[S]; P != 0; P = P->Next) SHOW_NODE(P->Cur);
    }
    printf(".\n");
  }
}

int EqualS(struct Subnode* A, struct Subnode* B) {
  for (; A != 0 && B != 0; A = A->Next, B = B->Next)
    if (A->Size != B->Size || A->Cur != B->Cur) return 0;
  return A == B;
}

void FreeSub(struct Subnode* A) {
  struct Subnode* Next;
  for (; A != 0; A = Next) {
    Next = A->Next; free(A);
    if (Next != 0 && --Next->Links > 0) break;
  }
}

unsigned AddSub(Symbol L, struct Subnode* P) {
  unsigned N, S, Size; struct Node* Nd;
  Size = L->Literal? 1: (P == 0)? 0: P->Size;
  for (N = NodeP; N < NodeE; N++) {
    Nd = &NodeTab[N];
    if (Nd->Sym == L && Nd->Size == Size) break;
  }
  if (N >= NodeE) {
    if ((NodeE&7) == 0)
      NodeTab = Reallocate(NodeTab, (NodeE + 8) * sizeof *NodeTab);
    N = NodeE++, Nd = &NodeTab[N];
    Nd->Sym = L, Nd->Size = Size;
    Nd->Start =
      L->Literal? Position - 1:
      (P == 0)?   Position:
      NodeTab[P->Cur].Start;
    Nd->Subs = 0, Nd->Sub = 0;
  }
  if (!L->Literal) {
    for (S = 0; S < Nd->Subs; S++)
      if (EqualS(Nd->Sub[S], P)) break;
    if (S >= Nd->Subs) {
      if ((Nd->Subs&3) == 0)
        Nd->Sub = Reallocate(Nd->Sub, (Nd->Subs + 4) * sizeof(struct Subnode*));
      Nd->Sub[Nd->Subs++] = P;
    } else FreeSub(P);
  }
  return N;
}

struct ZNode { unsigned Val; unsigned Size, *List; };
struct Vertex { struct State* Val; unsigned Start; unsigned Size; struct ZNode* *List; };
struct Vertex* VertTab; unsigned VertE, VertP;

void SHOW_W(unsigned W) {
  struct Vertex* V = &VertTab[W];
  printf(" v_%d_%ld", V->Start, V->Val - SList);
}

void SHOW_STACK(void) {
  unsigned int W, W1, Z; struct Vertex* V; struct ZNode* N;
  for (W = 0; W < VertE; W++) {
    V = &VertTab[W];
    SHOW_W(W);
    if (V->Size > 0) printf(" <=\t"); else putchar('\n');
    for (Z = 0; Z < V->Size; Z++) {
      if (Z > 0) putchar('\t'), putchar('\t');
      N = V->List[Z];
      printf(" ["), SHOW_NODE(N->Val), printf(" ] <=");
      for (W1 = 0; W1 < N->Size; W1++) SHOW_W(N->List[W1]);
      putchar('\n');
    }
  }
}

struct RRed { struct ZNode* Z; Symbol LHS; Rule RHS; };
struct RRed* REDS; unsigned RP, RE;
void AddRed(struct ZNode* Z, Symbol LHS, Rule RHS) {
  if ((RE&7) == 0) REDS = Reallocate(REDS, (RE + 8)*sizeof *REDS);
  REDS[RE].Z = Z, REDS[RE].LHS = LHS, REDS[RE].RHS = RHS;
  RE++;
}

struct ERed { unsigned W; Symbol LHS; };
struct ERed* EREDS; unsigned EP, EE;
void AddERed(unsigned W, Symbol LHS) {
  if ((EE&7) == 0) EREDS = Reallocate(EREDS, (EE + 8)*sizeof *EREDS);
  EREDS[EE].W = W, EREDS[EE].LHS = LHS;
  EE++;
}

unsigned AddQ(struct State* S) {
  unsigned V; struct Vertex* W; unsigned int E;
  for (V = VertP; V < VertE; V++) {
    W = &VertTab[V];
    if (W->Val == S) return V;
  }
  if ((VertE&7) == 0)
    VertTab = Reallocate(VertTab, (VertE + 8)*sizeof *VertTab);
  W = &VertTab[VertE];
  W->Val = S, W->Start = Position, W->Size = 0, W->List = 0;
  for (E = 0; E < S->Es; E++) AddERed(VertE, S->EList[E]);
  return VertE++;
}

void AddN(unsigned N, unsigned W) {
  struct Node* Nd = &NodeTab[N]; struct State* S = Next(VertTab[W].Val, Nd->Sym);
  struct Vertex* W1; unsigned Z; struct ZNode* Z1; unsigned int I;
  if (S == 0) return;
  W1 = &VertTab[AddQ(S)];
  for (Z = 0; Z < W1->Size; Z++) {
    Z1 = W1->List[Z];
    if (Z1->Val == N) break;
  }
  if (Z >= W1->Size) {
    struct Reduce* Rd; unsigned int R;
    if ((W1->Size&3) == 0)
      W1->List = Reallocate(W1->List, (W1->Size + 4) * sizeof(struct ZNode*));
    Z = W1->Size++;
    W1->List[Z] = Z1 = Allocate(sizeof *Z1);
    Z1->Val = N, Z1->Size = 0, Z1->List = 0;
    for (R = 0; R < S->Rs; R++)
      Rd = &S->RList[R], AddRed(Z1, Rd->LHS, Rd->RHS);
  }
  for (I = 0; I < Z1->Size; I++)
    if (Z1->List[I] == W) break;
  if (I >= Z1->Size) {
    if ((Z1->Size&3) == 0)
      Z1->List = Reallocate(Z1->List, (Z1->Size + 4)*sizeof *Z1->List);
    I = Z1->Size++;
    Z1->List[I] = W;
  }
}

struct Path { struct ZNode* Z; struct Subnode* P; };
struct Path* PathTab; unsigned PathE, PathP;

void AddLink(struct ZNode* Z, struct Subnode* P) {
  struct Path* PP; unsigned N = Z->Val; struct Node* Nd = &NodeTab[N];
  struct Subnode* NewP = Allocate(sizeof *NewP);
  NewP->Size = Nd->Size; if (P != 0) NewP->Size += P->Size, P->Links++;
  NewP->Cur = N, NewP->Next = P, NewP->Links = 0;
  P = NewP;
  if ((PathE&7) == 0)
    PathTab = Reallocate(PathTab, (PathE + 8) * sizeof *PathTab);
  PP = &PathTab[PathE++];
  PP->Z = Z, PP->P = P;
}

void Reduce1(struct ZNode* Z, Symbol L, Rule R) {
  unsigned PE, X; struct Path* PP; struct Subnode* P; struct Vertex* V;
  PathTab = 0, PathE = PathP = 0;
  AddLink(Z, 0);
  for (R++; *R != 0; R++)
    for (PE = PathE; PathP < PE; PathP++) {
      PP = &PathTab[PathP], Z = PP->Z, P = PP->P;
      for (unsigned W = 0; W < Z->Size; W++) {
        V = &VertTab[Z->List[W]];
        for (X = 0; X < V->Size; X++) AddLink(V->List[X], P);
      }
    }
  for (; PathP < PathE; PathP++) {
    unsigned N;
    PP = &PathTab[PathP], Z = PP->Z, P = PP->P;
    N = AddSub(L, P);
    for (unsigned W = 0; W < Z->Size; W++) AddN(N, Z->List[W]);
  }
  free(PathTab), PathP = PathE = 0;
}

struct Node* Parse(void) {
  Symbol Word; unsigned C, VP, N, W; struct Subnode* P;
  AddQ(&SList[0]);
  while (1) {
    /* REDUCE */
    while (EP < EE || RP < RE) {
      for (; RP < RE; RP++) Reduce1(REDS[RP].Z, REDS[RP].LHS, REDS[RP].RHS);
      for (; EP < EE; EP++) AddN(AddSub(EREDS[EP].LHS, 0), EREDS[EP].W);
    }
    /* SHIFT */
    Word = GetC();
    if (Word == 0) break;
    printf(" %s", Word->Name);
    P = Allocate(sizeof *P);
    P->Size = 1, P->Cur = AddSub(Word, 0), P->Next = 0, P->Links = 0;
    VP = VertP, VertP = VertE; NodeP = NodeE;
    if (Word->Rules == 0) { /* Treat the word as a new word. */
      Symbol S;
      for (S = FirstB; S != 0; S = S->Tail) {
        if (S->Rules > 0) continue;
        N = AddSub(S, P);
        for (W = VP; W < VertP; W++) AddN(N, W);
      }
    } else for (C = 0; C < Word->Rules; C++) {
      N = AddSub(*Word->RList[C], P);
      for (W = VP; W < VertP; W++) AddN(N, W);
    }
  }
  /* ACCEPT */
  putchar('\n');
  for (W = VertP; W < VertE; W++) {
    struct Vertex* V = &VertTab[W];
    if (V->Val->Final) return &NodeTab[V->List[0]->Val];
  }
  return 0;
}

void SetTables(void) {
  NodeTab = 0, NodeE = 0; Position = 0;
  VertTab = 0, VertE = 0;
  PathTab = 0, PathE = 0;
  REDS = 0, RP = RE = 0;
  EREDS = 0, EP = EE = 0;
  NodeP = NodeE; VertP = VertE;
}

void FreeTables(void) {
  unsigned int N, S, W, Z; struct Vertex* V; struct ZNode* ZN; struct Node* Nd;
  for (N = 0; N < NodeE; N++) {
    Nd = &NodeTab[N];
    for (S = 0; S < Nd->Subs; S++) FreeSub(Nd->Sub[S]);
    free(Nd->Sub);
  }
  free(NodeTab), NodeTab = 0, NodeE = NodeP = 0;
  for (W = 0; W < VertE; W++) {
    V = &VertTab[W];
    for (Z = 0; Z < V->Size; Z++) {
      ZN = V->List[Z]; free(ZN->List); free(ZN);
    }
    free(V->List);
  }
  free(VertTab), VertTab = 0, VertE = VertP = 0;
  free(REDS), RE = RP = 0;
  free(EREDS), EE = EP = 0;
}

int main(int argc, char **argv) {
  Symbol Start; struct Node* Nd; int Arg; char *AP;
  int DoC = 0, DoS = 0, DoH = 0;
  for (Arg = 1; Arg < argc; Arg++) {
    AP = argv[Arg];
    if (*AP++ != '-') break;
    for (; *AP != '\0'; AP++) switch (*AP) {
      case 's': DoS++; break;
      case 'c': DoC++; break;
      default: DoH++; break;
    }
  }
  if (DoH || Arg >= argc) {
    printf(
        "Usage: tom -sc? grammar\n"
        "    -c ...... display parsing table\n"
        "    -s ...... display parsing stack\n"
        "    -h/-? ... print this list\n"
      );
    exit(1);
  }
  if (!OPEN(argv[Arg]))
    fprintf(stderr, "Cannot open %s.\n", argv[Arg]), exit(1);
  Start = Grammar(&ERRORS);
  CLOSE();
  Check(&ERRORS);
  Generate(Start);
  if (DoC) SHOW_STATES();
  while (1) {
    SetTables();
    Nd = Parse();
    if (Position == 0) break;
    printf("\nParse Forest:\n");
    if (Nd != 0) {
      putchar('*'); SHOW_NODE(Nd - NodeTab); printf(".\n");
    }
    SHOW_FOREST();
    if (DoS) {
      printf("\nParse Stack:\n"), SHOW_STACK();
    }
    FreeTables();
  }
  return 0;
}

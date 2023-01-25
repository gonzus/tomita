#include <ctype.h>
#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "util.h"
#include "forest.h"

// a Subnode, stored as a linked list in a Node and a Path
struct Subnode {
  unsigned Size;
  unsigned Cur;
  unsigned ref_cnt;            // reference counted
  struct Subnode* Next;      // link to next Subnode
};

// a Node
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

// a Vertex
struct Vertex {
  struct State* Val;
  unsigned Start;
  unsigned Size;
  struct ZNode** List;
};

// a Regular Reduction
struct RRed {
  struct ZNode* Z;
  Symbol* LHS;               // left-hand side symbol
  Symbol** RHS;              // right-hand side rule (set of symbols)
};

// an Empty Reduction
struct ERed {
  unsigned W;                // vertex
  Symbol* LHS;               // left-hand side symbol
};

static void forest_prepare(Forest* forest);
static void forest_cleanup(Forest* forest);
static void forest_show_vertex(Forest* forest, unsigned W);
static Symbol* next_symbol(Forest* forest);

static void subnode_free(struct Subnode* A);
static int subnode_equal(struct Subnode* A, struct Subnode* B);
static unsigned subnode_add(Forest* forest, Symbol* L, struct Subnode* P);

static unsigned AddQ(Forest* forest, struct State* S);
static void ReduceOne(Forest* forest, struct ZNode* Z, Symbol* L, Symbol** R);
static void AddN(Forest* forest, unsigned N, unsigned W);
static void AddLink(Forest* forest, struct ZNode* Z, struct Subnode* P);
static void AddRRed(Forest* forest, struct ZNode* Z, Symbol* LHS, Symbol** RHS);
static void AddERed(Forest* forest, unsigned W, Symbol* LHS);
static struct State* NextState(Forest* forest, struct State* Q, Symbol* Sym);

Forest* forest_create(LexerI* lexer, Parser* parser) {
  Forest* forest = 0;
  MALLOC(Forest, forest);
  forest->parser = parser;
  forest->lexer = lexer;
  return forest;
}

void forest_destroy(Forest* forest) {
  forest_cleanup(forest);
  FREE(forest);
}

void forest_show(Forest* forest) {
  for (unsigned N = 0; N < forest->node_cap; ++N) {
    struct Node* Nd = &forest->node_table[N];
    if (Nd->Sym->literal) continue;
    forest_show_node(forest, Nd);
    if (Nd->Subs > 0) {
      struct Subnode* P = Nd->Sub[0];
      // we were not checking P in the next if
      // fix by gonzo
      if (P && forest->node_table[P->Cur].Sym->literal) {
        putchar(':');
      } else {
        printf(" =");
      }
    }
    for (unsigned S = 0; S < Nd->Subs; ++S) {
      if (S > 0) {
        printf(" |");
      }
      for (struct Subnode* P = Nd->Sub[S]; P != 0; P = P->Next) {
        struct Node* node = &forest->node_table[P->Cur];
        forest_show_node(forest, node);
      }
    }
    printf(".\n");
  }
}

void forest_show_node(Forest* forest, struct Node* node) {
  UNUSED(forest);
  Slice name = node->Sym->name;
  if (node->Sym->literal) {
    printf(" \"%.*s\"", name.len, name.ptr);
  }
  else {
    printf(" %.*s_%d_%d", name.len, name.ptr, node->Start, node->Start + node->Size);
  }
}

void forest_show_stack(Forest* forest) {
  for (unsigned W = 0; W < forest->vert_cap; ++W) {
    forest_show_vertex(forest, W);
    struct Vertex* V = &forest->vert_table[W];
    if (V->Size > 0) {
      printf(" <=\t");
    } else {
      putchar('\n');
    }
    for (unsigned Z = 0; Z < V->Size; ++Z) {
      if (Z > 0) {
        printf("\t\t");
      }
      struct ZNode* N = V->List[Z];
      struct Node* node = &forest->node_table[N->Val];
      printf(" [");
      forest_show_node(forest, node);
      printf(" ] <=");
      for (unsigned W1 = 0; W1 < N->Size; ++W1) {
        forest_show_vertex(forest, N->List[W1]);
      }
      putchar('\n');
    }
  }
}

static void forest_show_vertex(Forest* forest, unsigned W) {
  struct Vertex* V = &forest->vert_table[W];
  printf(" v_%d_%ld", V->Start, V->Val - forest->parser->SList);
}

static void forest_prepare(Forest* forest) {
  if (forest->prepared) return;
  forest->prepared = 1;
  LOG_INFO("preparing forest");

  forest->position = 0;
  forest->node_table = 0;
  forest->node_cap = 0;
  forest->node_pos = 0;
  forest->vert_table = 0;
  forest->vert_cap = 0;
  forest->vert_pos = 0;
  forest->path_table = 0;
  forest->path_cap = 0;
  forest->rr_table = 0;
  forest->rr_cap = 0;
  forest->rr_pos = 0;
  forest->er_table = 0;
  forest->ee_cap = 0;
  forest->ee_pos = 0;
}

static void forest_cleanup(Forest* forest) {
  if (!forest->prepared) return;
  forest->prepared = 0;
  LOG_INFO("cleaning up forest");

  for (unsigned N = 0; N < forest->node_cap; ++N) {
    struct Node* Nd = &forest->node_table[N];
    for (unsigned S = 0; S < Nd->Subs; ++S) {
      subnode_free(Nd->Sub[S]);
    }
    FREE(Nd->Sub);
  }
  FREE(forest->node_table);
  forest->node_cap = 0;
  forest->node_pos = 0;
  for (unsigned W = 0; W < forest->vert_cap; ++W) {
    struct Vertex* V = &forest->vert_table[W];
    for (unsigned Z = 0; Z < V->Size; ++Z) {
      struct ZNode*  ZN = V->List[Z];
      FREE(ZN->List);
      FREE(ZN);
    }
    FREE(V->List);
  }
  FREE(forest->vert_table);
  forest->vert_cap = forest->vert_pos = 0;
  FREE(forest->rr_table);
  forest->rr_cap = forest->rr_pos = 0;
  FREE(forest->er_table);
  forest->ee_cap = forest->ee_pos = 0;
}

struct Node* forest_parse(Forest* forest, Slice text) {
  forest_cleanup(forest);
  forest_prepare(forest);

  LexerI* lexer = forest->lexer;
  lexer->set_input(lexer->context, text);
  AddQ(forest, &forest->parser->SList[0]);
  while (1) {
    /* REDUCE */
    while (forest->ee_pos < forest->ee_cap || forest->rr_pos < forest->rr_cap) {
      for (; forest->rr_pos < forest->rr_cap; ++forest->rr_pos) {
        ReduceOne(forest, forest->rr_table[forest->rr_pos].Z, forest->rr_table[forest->rr_pos].LHS, forest->rr_table[forest->rr_pos].RHS);
      }
      for (; forest->ee_pos < forest->ee_cap; ++forest->ee_pos) {
        AddN(forest, subnode_add(forest, forest->er_table[forest->ee_pos].LHS, 0), forest->er_table[forest->ee_pos].W);
      }
    }
    /* SHIFT */
    Symbol* Word = next_symbol(forest);
    if (Word == 0) break;
    // printf(" %.*s", Word->name.len, Word->name.ptr);
    struct Subnode* P = 0;
    MALLOC(struct Subnode, P);
    P->Size = 1;
    P->Cur = subnode_add(forest, Word, 0);
    P->Next = 0;
    P->ref_cnt = 0;
    unsigned VP = forest->vert_pos;
    forest->vert_pos = forest->vert_cap;
    forest->node_pos = forest->node_cap;
    if (Word->rule_count == 0) { /* Treat the word as a new word. */
      for (Symbol* S = forest->parser->grammar->first; S != 0; S = S->tail) {
        if (S->rule_count > 0) continue;
        unsigned N = subnode_add(forest, S, P);
        for (unsigned W = VP; W < forest->vert_pos; ++W) {
          AddN(forest, N, W);
        }
      }
    } else {
      for (unsigned C = 0; C < Word->rule_count; ++C) {
        unsigned N = subnode_add(forest, *Word->rules[C], P);
        for (unsigned W = VP; W < forest->vert_pos; ++W) {
          AddN(forest, N, W);
        }
      }
    }
  }
  /* ACCEPT */
  // printf("\n");
  for (unsigned W = forest->vert_pos; W < forest->vert_cap; ++W) {
    struct Vertex* V = &forest->vert_table[W];
    if (V->Val->Final) {
      return &forest->node_table[V->List[0]->Val];
    }
  }
  return 0;
}

static void subnode_free(struct Subnode* A) {
  while (A != 0) {
    struct Subnode* Next = A->Next;
    UNREF(A);
    if (A) break;
    A = Next;
  }
}

static unsigned subnode_add(Forest* forest, Symbol* L, struct Subnode* P) {
  unsigned N, S, Size; struct Node* Nd;
  Size = L->literal? 1: (P == 0)? 0: P->Size;
  for (N = forest->node_pos; N < forest->node_cap; ++N) {
    Nd = &forest->node_table[N];
    if (Nd->Sym == L && Nd->Size == Size) break;
  }
  if (N >= forest->node_cap) {
    if ((forest->node_cap & 7) == 0) {
      REALLOC(struct Node, forest->node_table, forest->node_cap + 8);
    }
    N = forest->node_cap++;
    Nd = &forest->node_table[N];
    Nd->Sym = L;
    Nd->Size = Size;
    Nd->Start = L->literal ? forest->position - 1
              : (P == 0)   ? forest->position
              : forest->node_table[P->Cur].Start;
    Nd->Subs = 0;
    Nd->Sub = 0;
  }
  if (!L->literal) {
    for (S = 0; S < Nd->Subs; ++S) {
      if (subnode_equal(Nd->Sub[S], P)) break;
    }
    if (S >= Nd->Subs) {
      if ((Nd->Subs & 3) == 0) {
        REALLOC(struct Subnode*, Nd->Sub, Nd->Subs + 4);
      }
      // we are adding a reference to this Subnode, increment its reference count
      // fix by gonzo
      Nd->Sub[Nd->Subs++] = REF(P);
    } else {
      subnode_free(P);
    }
  }
  return N;
}

static int subnode_equal(struct Subnode* A, struct Subnode* B) {
  for (; A != 0 && B != 0; A = A->Next, B = B->Next)
    if (A->Size != B->Size || A->Cur != B->Cur) return 0;
  return A == B; // NOTE: why?
}

static unsigned AddQ(Forest* forest, struct State* S) {
  for (unsigned V = forest->vert_pos; V < forest->vert_cap; ++V) {
    struct Vertex* W = &forest->vert_table[V];
    if (W->Val == S) return V;
  }
  if ((forest->vert_cap & 7) == 0) {
    REALLOC(struct Vertex, forest->vert_table, forest->vert_cap + 8);
  }
  struct Vertex* W = &forest->vert_table[forest->vert_cap];
  W->Val = S;
  W->Start = forest->position;
  W->Size = 0;
  W->List = 0;
  for (unsigned E = 0; E < S->Es; ++E) {
    AddERed(forest, forest->vert_cap, S->EList[E]);
  }
  return forest->vert_cap++;
}

static void ReduceOne(Forest* forest, struct ZNode* Z, Symbol* L, Symbol** R) {
  forest->path_table = 0;
  forest->path_cap = 0;
  unsigned PathP = 0;
  AddLink(forest, Z, 0);
  for (++R; *R != 0; ++R) {
    for (unsigned PE = forest->path_cap; PathP < PE; ++PathP) {
      struct Path* PP = &forest->path_table[PathP];
      struct Subnode* P = PP->P;
      Z = PP->Z;
      for (unsigned W = 0; W < Z->Size; ++W) {
        struct Vertex* V = &forest->vert_table[Z->List[W]];
        for (unsigned X = 0; X < V->Size; ++X) {
          AddLink(forest, V->List[X], P);
        }
      }
    }
  }
  for (; PathP < forest->path_cap; ++PathP) {
    struct Path* PP = &forest->path_table[PathP];
    struct Subnode* P = PP->P;
    Z = PP->Z;
    unsigned N = subnode_add(forest, L, P);
    for (unsigned W = 0; W < Z->Size; ++W) {
      AddN(forest, N, Z->List[W]);
    }
  }
  FREE(forest->path_table);
  forest->path_cap = 0;
}

static void AddN(Forest* forest, unsigned N, unsigned W) {
  struct Node* Nd = &forest->node_table[N];
  struct State* S = NextState(forest, forest->vert_table[W].Val, Nd->Sym);
  struct Vertex* W1;
  unsigned Z;
  struct ZNode* Z1;
  unsigned int I;
  if (S == 0) return;
#if 0
  // on my M1 laptop, this does not work...
  W1 = &forest->vert_table[AddQ(forest, S)];
#else
  // ... but this does -- compiler bug?
  // fix by gonzo
  unsigned pos = AddQ(forest, S);
  W1 = &forest->vert_table[pos];
#endif
  for (Z = 0; Z < W1->Size; ++Z) {
    Z1 = W1->List[Z];
    if (Z1->Val == N) break;
  }
  if (Z >= W1->Size) {
    struct Reduce* Rd;
    unsigned int R;
    if ((W1->Size & 3) == 0) {
      REALLOC(struct ZNode*, W1->List, W1->Size + 4);
    }
    Z = W1->Size++;
    Z1 = 0;
    MALLOC(struct ZNode, Z1);
    W1->List[Z] = Z1;
    Z1->Val = N;
    Z1->Size = 0;
    Z1->List = 0;
    for (R = 0; R < S->Rs; ++R) {
      Rd = &S->RList[R], AddRRed(forest, Z1, Rd->LHS, Rd->RHS);
    }
  }
  for (I = 0; I < Z1->Size; ++I) {
    if (Z1->List[I] == W) break;
  }
  if (I >= Z1->Size) {
    if ((Z1->Size & 3) == 0) {
      REALLOC(unsigned, Z1->List, Z1->Size + 4);
    }
    I = Z1->Size++;
    Z1->List[I] = W;
  }
}

static void AddRRed(Forest* forest, struct ZNode* Z, Symbol* LHS, Symbol** RHS) {
  if ((forest->rr_cap & 7) == 0) {
    REALLOC(struct RRed, forest->rr_table, forest->rr_cap + 8);
  }
  forest->rr_table[forest->rr_cap].Z = Z;
  forest->rr_table[forest->rr_cap].LHS = LHS;
  forest->rr_table[forest->rr_cap].RHS = RHS;
  ++forest->rr_cap;
}

static void AddERed(Forest* forest, unsigned W, Symbol* LHS) {
  if ((forest->ee_cap & 7) == 0) {
    REALLOC(struct ERed, forest->er_table, forest->ee_cap + 8);
  }
  forest->er_table[forest->ee_cap].W = W;
  forest->er_table[forest->ee_cap].LHS = LHS;
  ++forest->ee_cap;
}

static void AddLink(Forest* forest, struct ZNode* Z, struct Subnode* P) {
  struct Path* PP;
  unsigned N = Z->Val;
  struct Node* Nd = &forest->node_table[N];
  struct Subnode* NewP = 0;
  MALLOC(struct Subnode, NewP);
  NewP->Size = Nd->Size;
  if (P != 0) {
    NewP->Size += P->Size;
    REF(P);
  }
  NewP->Cur = N;
  NewP->Next = P;
  NewP->ref_cnt = 0;
  P = NewP;
  if ((forest->path_cap & 7) == 0) {
    REALLOC(struct Path, forest->path_table, forest->path_cap + 8);
  }
  PP = &forest->path_table[forest->path_cap++];
  PP->Z = Z;
  PP->P = P;
}

static struct State* NextState(Forest* forest, struct State* Q, Symbol* Sym) {
  for (unsigned S = 0; S < Q->Ss; ++S) {
    struct Shift* Sh = &Q->SList[S];
    if (Sh->X == Sym) return &forest->parser->SList[Sh->Q];
  }
  return 0;
}

static Symbol* next_symbol(Forest* forest) {
  Symbol* sym = 0;
  do {
    LexerI* lexer = forest->lexer;
    if (!lexer) {
      LOG_WARN("could not get lexer from forest");
      break;
    }
    sym = lexer->next_token(lexer->context);
    if (!sym) {
      LOG_WARN("could not get next token from lexer");
      break;
    }

    LOG_INFO("symbol %p [%.*s]", sym, sym->name.len, sym->name.ptr);
    ++forest->position;
  } while (0);
  return sym;
}

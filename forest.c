#include <ctype.h>
#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "symbol.h"
#include "symtab.h"
#include "parser.h"
#include "forest.h"
#include "tomita.h"

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
  struct Reduce* Rd;         // the reduce rule
};

// an Empty Reduction
struct ERed {
  unsigned W;                // vertex
  Symbol* LHS;               // left-hand side symbol
};

static void forest_prepare(Forest* forest);
static void forest_show_vertex(Forest* forest, unsigned W);
static unsigned next_symbol(Forest* forest, Slice text, unsigned pos, Symbol** symbol);
static void node_show(struct Node* node);

static void subnode_free(struct Subnode* A);
static int subnode_equal(struct Subnode* A, struct Subnode* B);
static unsigned subnode_add(Forest* forest, Symbol* L, struct Subnode* P);

static unsigned AddQ(Forest* forest, struct State* S);
static void ReduceOne(Forest* forest, struct ZNode* Z, struct Reduce* Rd);
static void AddN(Forest* forest, unsigned N, unsigned W);
static void AddLink(Forest* forest, struct ZNode* Z, struct Subnode* P);
static void AddRRed(Forest* forest, struct ZNode* Z, struct Reduce* Rd);
static void AddERed(Forest* forest, unsigned W, Symbol* LHS);
static struct State* NextState(Forest* forest, struct State* Q, Symbol* symbol);

Forest* forest_create(struct Parser* parser, ForestCallbacks* cb, void* ctx) {
  Forest* forest = 0;
  MALLOC(Forest, forest);
  forest->parser = parser;
  forest->cb = cb;
  forest->ctx = ctx;
  return forest;
}

void forest_destroy(Forest* forest) {
  forest_clear(forest);
  FREE(forest);
}

void forest_show(Forest* forest) {
  printf("%c%c FOREST\n", FORMAT_COMMENT, FORMAT_COMMENT);
  for (unsigned N = 0; N < forest->node_cap; ++N) {
    struct Node* Nd = &forest->node_table[N];
    if (Nd->symbol->literal) continue;
    printf("%c", Nd == forest->root ? '*' : ' ');
    node_show(Nd);
    if (Nd->sub_cap > 0) {
      struct Subnode* P = Nd->sub_table[0];
      // we were not checking P in the next if
      // fix by gonzo
      if (P && forest->node_table[P->Cur].symbol->literal) {
        putchar(':');
      } else {
        printf(" =");
      }
    }
    for (unsigned S = 0; S < Nd->sub_cap; ++S) {
      if (S > 0) {
        printf(" |");
      }
      for (struct Subnode* P = Nd->sub_table[S]; P != 0; P = P->next) {
        struct Node* node = &forest->node_table[P->Cur];
        node_show(node);
      }
    }
    printf(".\n");
  }
}

static void node_show(struct Node* node) {
  Slice name = node->symbol->name;
  if (node->symbol->literal) {
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
      printf(" <-\t");
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
      node_show(node);
      printf(" ] <-");
      for (unsigned W1 = 0; W1 < N->Size; ++W1) {
        forest_show_vertex(forest, N->List[W1]);
      }
      putchar('\n');
    }
  }
}

static void forest_show_vertex(Forest* forest, unsigned W) {
  struct Vertex* V = &forest->vert_table[W];
  printf(" v_%d_%ld", V->Start, V->Val - forest->parser->state_table);
}

static void forest_prepare(Forest* forest) {
  if (forest->prepared) return;
  forest->prepared = 1;
  LOG_DEBUG("preparing forest");

  forest->position = 0;
  forest->root = 0;
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
  forest->er_cap = 0;
  forest->er_pos = 0;
}

void forest_clear(Forest* forest) {
  if (!forest->prepared) return;
  forest->prepared = 0;
  LOG_DEBUG("cleaning up forest");

  forest->root = 0;
  for (unsigned N = 0; N < forest->node_cap; ++N) {
    struct Node* Nd = &forest->node_table[N];
    for (unsigned S = 0; S < Nd->sub_cap; ++S) {
      subnode_free(Nd->sub_table[S]);
    }
    FREE(Nd->sub_table);
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
  forest->er_cap = forest->er_pos = 0;
}

unsigned forest_parse(Forest* forest, Slice text) {
  forest_clear(forest);
  forest_prepare(forest);
  AddQ(forest, &forest->parser->state_table[0]);
  unsigned pos = 0;
  while (1) {
    /* REDUCE */
    while (forest->er_pos < forest->er_cap || forest->rr_pos < forest->rr_cap) {
      for (; forest->rr_pos < forest->rr_cap; ++forest->rr_pos) {
        struct RRed* rr = &forest->rr_table[forest->rr_pos];
        ReduceOne(forest, rr->Z, rr->Rd);
      }
      for (; forest->er_pos < forest->er_cap; ++forest->er_pos) {
        // printf("REDUCE: AddN\n");
        AddN(forest, subnode_add(forest, forest->er_table[forest->er_pos].LHS, 0), forest->er_table[forest->er_pos].W);
      }
    }
    /* SHIFT */
    Symbol* Word = 0;
    pos = next_symbol(forest, text, pos, &Word);
    if (Word == 0) break;
    // printf("PUSH [%.*s]\n", Word->name.len, Word->name.ptr);
    if (forest->cb && forest->ctx) {
      forest->cb->new_token(forest->ctx, Word->name);
    }
    // symbol_show(Word, 0, 0);
    struct Subnode* P = 0;
    MALLOC(struct Subnode, P);
    P->Size = 1;
    P->Cur = subnode_add(forest, Word, 0);
    P->next = 0;
    P->ref_cnt = 0;
    unsigned VP = forest->vert_pos;
    forest->vert_pos = forest->vert_cap;
    forest->node_pos = forest->node_cap;
    if (Word->rs_cap == 0) { /* Treat the word as a new word. */
      for (Symbol* S = forest->parser->symtab->first; S != 0; S = S->nxt_list) {
        if (S->rs_cap > 0) continue;
        unsigned N = subnode_add(forest, S, P);
        for (unsigned W = VP; W < forest->vert_pos; ++W) {
          // printf("SHIFT: AddN NEW %u %u\n", N, W);
          AddN(forest, N, W);
        }
      }
    } else {
      for (unsigned C = 0; C < Word->rs_cap; ++C) {
        unsigned N = subnode_add(forest, *Word->rs_table[C].rules, P);
        for (unsigned W = VP; W < forest->vert_pos; ++W) {
          // printf("SHIFT: AddN OLD %u %u\n", N, W);
          AddN(forest, N, W);
        }
      }
    }
  }
  /* ACCEPT */
  // printf("\n");
  for (unsigned W = forest->vert_pos; W < forest->vert_cap; ++W) {
    struct Vertex* V = &forest->vert_table[W];
    if (V->Val->final) {
      // printf("ACCEPT\n");
      if (forest->cb && forest->ctx) {
        forest->cb->accept(forest->ctx);
      }
      forest->root = &forest->node_table[V->List[0]->Val];
      return 0;
    }
  }
  return 1;
}

static void subnode_free(struct Subnode* A) {
  while (A != 0) {
    struct Subnode* next = A->next;
    UNREF(A);
    if (A) break;
    A = next;
  }
}

static unsigned subnode_add(Forest* forest, Symbol* L, struct Subnode* P) {
  unsigned N, S, Size; struct Node* Nd;
  Size = L->literal ? 1
       : (P == 0)   ? 0
       : P->Size;
  for (N = forest->node_pos; N < forest->node_cap; ++N) {
    Nd = &forest->node_table[N];
    if (Nd->symbol == L && Nd->Size == Size) break;
  }
  if (N >= forest->node_cap) {
    TABLE_CHECK_GROW(forest->node_table, forest->node_cap, 8, struct Node);
    N = forest->node_cap++;
    Nd = &forest->node_table[N];
    Nd->symbol = L;
    Nd->Size = Size;
    Nd->Start = L->literal ? forest->position - 1
              : (P == 0)   ? forest->position
              : forest->node_table[P->Cur].Start;
    Nd->sub_cap = 0;
    Nd->sub_table = 0;
  }
  if (!L->literal) {
    for (S = 0; S < Nd->sub_cap; ++S) {
      if (subnode_equal(Nd->sub_table[S], P)) break;
    }
    if (S >= Nd->sub_cap) {
      TABLE_CHECK_GROW(Nd->sub_table, Nd->sub_cap, 4, struct Subnode*);
      // we are adding a reference to this Subnode, increment its reference count
      // fix by gonzo
      Nd->sub_table[Nd->sub_cap++] = REF(P);
    } else {
      subnode_free(P);
    }
  }
  return N;
}

static int subnode_equal(struct Subnode* A, struct Subnode* B) {
  for (; A != 0 && B != 0; A = A->next, B = B->next)
    if (A->Size != B->Size || A->Cur != B->Cur) return 0;
  return A == B; // NOTE: why?
}

static unsigned AddQ(Forest* forest, struct State* S) {
  for (unsigned V = forest->vert_pos; V < forest->vert_cap; ++V) {
    struct Vertex* W = &forest->vert_table[V];
    if (W->Val == S) return V;
  }
  TABLE_CHECK_GROW(forest->vert_table, forest->vert_cap, 8, struct Vertex);
  struct Vertex* W = &forest->vert_table[forest->vert_cap];
  W->Val = S;
  W->Start = forest->position;
  W->Size = 0;
  W->List = 0;
  for (unsigned E = 0; E < S->er_cap; ++E) {
    AddERed(forest, forest->vert_cap, S->er_table[E]);
  }
  return forest->vert_cap++;
}

static void ReduceOne(Forest* forest, struct ZNode* Z, struct Reduce* Rd) {
  RuleSet* rs = &Rd->rs;
  if (forest->cb && forest->ctx) {
    forest->cb->reduce_rule(forest->ctx, rs);
  }

  Symbol* L = Rd->lhs;
  Symbol** R = rs->rules;
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
  struct State* S = NextState(forest, forest->vert_table[W].Val, Nd->symbol);
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
    TABLE_CHECK_GROW(W1->List, W1->Size, 4, struct ZNode*);
    Z = W1->Size++;
    Z1 = 0;
    MALLOC(struct ZNode, Z1);
    W1->List[Z] = Z1;
    Z1->Val = N;
    Z1->Size = 0;
    Z1->List = 0;
    for (R = 0; R < S->rr_cap; ++R) {
      Rd = &S->rr_table[R];
      // printf("AddRR for ruleset %u\n", Rd->rs.index);
      AddRRed(forest, Z1, Rd);
    }
  }
  for (I = 0; I < Z1->Size; ++I) {
    if (Z1->List[I] == W) break;
  }
  if (I >= Z1->Size) {
    TABLE_CHECK_GROW(Z1->List, Z1->Size, 4, unsigned);
    I = Z1->Size++;
    Z1->List[I] = W;
  }
}

static void AddRRed(Forest* forest, struct ZNode* Z, struct Reduce* Rd) {
  TABLE_CHECK_GROW(forest->rr_table, forest->rr_cap, 8, struct RRed);
  forest->rr_table[forest->rr_cap].Z = Z;
  forest->rr_table[forest->rr_cap].Rd = Rd;
  ++forest->rr_cap;
}

static void AddERed(Forest* forest, unsigned W, Symbol* LHS) {
  TABLE_CHECK_GROW(forest->er_table, forest->er_cap, 8, struct ERed);
  forest->er_table[forest->er_cap].W = W;
  forest->er_table[forest->er_cap].LHS = LHS;
  ++forest->er_cap;
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
  NewP->next = P;
  NewP->ref_cnt = 0;
  P = NewP;
  TABLE_CHECK_GROW(forest->path_table, forest->path_cap, 8, struct Path);
  PP = &forest->path_table[forest->path_cap++];
  PP->Z = Z;
  PP->P = P;
}

static struct State* NextState(Forest* forest, struct State* Q, Symbol* symbol) {
  for (unsigned S = 0; S < Q->ss_cap; ++S) {
    struct Shift* Sh = &Q->ss_table[S];
    if (Sh->symbol == symbol) return &forest->parser->state_table[Sh->state];
  }
  return 0;
}

static unsigned next_symbol(Forest* forest, Slice text, unsigned pos, Symbol** symbol) {
  *symbol = 0;
  do {
    // skip whitespace
    while (pos < text.len && (text.ptr[pos] == ' ' || text.ptr[pos] == '\t')) ++pos;

    // ran out? done
    if (pos >= text.len) break;

    // work line by line -- good for an interactive session
    if (text.ptr[pos] == '\n') {
      ++pos;
      break;
    }

    // gather non-space characters as symbol name
    unsigned beg = pos;
    while (pos < text.len && !isspace(text.ptr[pos])) ++pos;
    Slice name = slice_from_memory(text.ptr + beg, pos - beg);
    *symbol = symtab_lookup(forest->parser->symtab, name, 1, 1);
    ++forest->position;
  } while (0);

  LOG_DEBUG("symbol %p [%.*s]", *symbol, *symbol ? (*symbol)->name.len : 0, *symbol ? (*symbol)->name.ptr : 0);
  return pos;
}

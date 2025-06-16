#include <ctype.h>
#include <stdio.h>
#include "log.h"
#include "mem.h"
#include "symbol.h"
#include "symtab.h"
#include "parser.h"
#include "forest.h"
#include "tomita.h"

// a reference-counted subnode, part of a node
// it represents a possible parsed branch for the node
struct Subnode {
  unsigned Size;
  unsigned Cur;
  unsigned ref_cnt;          // reference count
  struct Subnode* next;      // link to next Subnode
};

struct ZNode {
  unsigned Index;
  unsigned Size;
  unsigned* List;
};

struct Path {
  struct ZNode* Zn;
  struct Subnode* Sn;
};

// a Vertex
struct Vertex {
  struct ParserState* State;
  unsigned Start;
  unsigned Size;
  struct ZNode** List;
};

// a Regular Reduction
struct RRed {
  struct ZNode* Zn;
  struct Reduce* Rd;         // the reduce rule
};

// an Empty Reduction
struct ERed {
  unsigned vertex_index;     // vertex
  Symbol* LHS;               // left-hand side symbol
};

static void forest_prepare(Forest* forest);
static void forest_show_vertex(Forest* forest, unsigned vertex_index);
static unsigned next_symbol(Forest* forest, Slice text, unsigned pos, Symbol** symbol);
static void node_show(struct Node* node);

static void subnode_free(struct Subnode* Sn);
static int subnode_equal(struct Subnode* l, struct Subnode* r);
static unsigned subnode_add(Forest* forest, Symbol* symbol, struct Subnode* Sn);

static unsigned AddQ(Forest* forest, struct ParserState* state);
static void ReduceOne(Forest* forest, struct RRed* rr);
static void AddN(Forest* forest, unsigned N, unsigned vertex_index);
static void AddLink(Forest* forest, struct ZNode* Zn, struct Subnode* Sn);
static void AddRRed(Forest* forest, struct ZNode* Zn, struct Reduce* Rd);
static void AddERed(Forest* forest, unsigned vertex_index, Symbol* LHS);
static struct ParserState* NextState(Forest* forest, struct ParserState* state, Symbol* symbol);

Forest* forest_create(struct Parser* parser, ForestCallbacks* fcb, void* fct) {
  Forest* forest = 0;
  MALLOC(Forest, forest);
  forest->parser = parser;
  forest->fcb = fcb;
  forest->fct = fct;
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
      struct Subnode* Sn = Nd->sub_table[0];
      // we were not checking Sn in the next if
      // fix by gonzo
      if (Sn && forest->node_table[Sn->Cur].symbol->literal) {
        putchar(':');
      } else {
        printf(" =");
      }
    }
    for (unsigned S = 0; S < Nd->sub_cap; ++S) {
      if (S > 0) {
        printf(" |");
      }
      for (struct Subnode* Sn = Nd->sub_table[S]; Sn != 0; Sn = Sn->next) {
        struct Node* node = &forest->node_table[Sn->Cur];
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
  for (unsigned forest_vertex_index = 0; forest_vertex_index < forest->vert_cap; ++forest_vertex_index) {
    forest_show_vertex(forest, forest_vertex_index);
    struct Vertex* V = &forest->vert_table[forest_vertex_index];
    if (V->Size > 0) {
      printf(" <-\t");
    } else {
      putchar('\n');
    }
    for (unsigned vertex_index = 0; vertex_index < V->Size; ++vertex_index) {
      if (vertex_index > 0) {
        printf("\t\t");
      }
      struct ZNode* N = V->List[vertex_index];
      struct Node* node = &forest->node_table[N->Index];
      printf(" [");
      node_show(node);
      printf(" ] <-");
      for (unsigned adj_index = 0; adj_index < N->Size; ++adj_index) {
        forest_show_vertex(forest, N->List[adj_index]);
      }
      putchar('\n');
    }
  }
}

static void forest_show_vertex(Forest* forest, unsigned vertex_index) {
  struct Vertex* V = &forest->vert_table[vertex_index];
  printf(" v_%d_%ld", V->Start, V->State - forest->parser->states);
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
  for (unsigned forest_vertex_index = 0; forest_vertex_index < forest->vert_cap; ++forest_vertex_index) {
    struct Vertex* V = &forest->vert_table[forest_vertex_index];
    for (unsigned vertex_index = 0; vertex_index < V->Size; ++vertex_index) {
      struct ZNode*  ZN = V->List[vertex_index];
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

static void add_shift_nodes(Forest* forest, struct Subnode* Sn, unsigned vertex_pos, Symbol* symbol) {
  unsigned N = subnode_add(forest, symbol, Sn);
  for (unsigned vertex_index = vertex_pos; vertex_index < forest->vert_pos; ++vertex_index) {
    AddN(forest, N, vertex_index);
  }
}

unsigned forest_parse(Forest* forest, Slice text) {
  forest_clear(forest);
  forest_prepare(forest);
  AddQ(forest, &forest->parser->states[0]);
  unsigned pos = 0;
  while (1) {
    /* REDUCE as much as possible */
    while (forest->er_pos < forest->er_cap || forest->rr_pos < forest->rr_cap) {
      // Run all possible regular reductions
      for (; forest->rr_pos < forest->rr_cap; ++forest->rr_pos) {
        struct RRed* rr = &forest->rr_table[forest->rr_pos];
        // printf("Regular Reduce\n");
        ReduceOne(forest, rr);
      }
      // Run all possible epsilon reductions
      for (; forest->er_pos < forest->er_cap; ++forest->er_pos) {
        // printf("Epsilon Reduce\n");
        unsigned N = subnode_add(forest, forest->er_table[forest->er_pos].LHS, 0);
        AddN(forest, N, forest->er_table[forest->er_pos].vertex_index);
      }
    }

    /* SHIFT next symbol; if none, stop */
    Symbol* Word = 0;
    pos = next_symbol(forest, text, pos, &Word);
    if (Word == 0) break;
    // printf("PUSH [%.*s]\n", Word->name.len, Word->name.ptr);
    if (forest->fcb && forest->fct) {
      forest->fcb->new_token(forest->fct, Word->name);
    }
    // symbol_show(Word, 0, 0);
    struct Subnode* Sn = 0;
    MALLOC(struct Subnode, Sn);
    Sn->Size = 1;
    Sn->Cur = subnode_add(forest, Word, 0);
    Sn->next = 0;
    Sn->ref_cnt = 0;
    unsigned VP = forest->vert_pos;
    forest->vert_pos = forest->vert_cap;
    forest->node_pos = forest->node_cap;
    if (Word->rs_cap == 0) {
      // Treat the word as a new word.
      for (Symbol* symbol = forest->parser->symtab->first; symbol != 0; symbol = symbol->nxt_list) {
        if (symbol->rs_cap > 0) continue;
        // printf("SHIFT new word\n");
        add_shift_nodes(forest, Sn, VP, symbol);
      }
    } else {
      for (unsigned rs_index = 0; rs_index < Word->rs_cap; ++rs_index) {
        Symbol* symbol = *Word->rs_table[rs_index].rules;
        // printf("SHIFT existing word\n");
        add_shift_nodes(forest, Sn, VP, symbol);
      }
    }
  }

  /* ACCEPT if there is a final state */
  // printf("\n");
  for (unsigned vertex_index = forest->vert_pos; vertex_index < forest->vert_cap; ++vertex_index) {
    struct Vertex* V = &forest->vert_table[vertex_index];
    if (V->State->final) {
      // printf("ACCEPT\n");
      if (forest->fcb && forest->fct) {
        forest->fcb->accept(forest->fct);
      }
      forest->root = &forest->node_table[V->List[0]->Index];
      return 0;
    }
  }
  return 1;
}

static void subnode_free(struct Subnode* Sn) {
  while (Sn != 0) {
    struct Subnode* next = Sn->next;
    UNREF(Sn);
    if (Sn) break;
    Sn = next;
  }
}

static unsigned subnode_add(Forest* forest, Symbol* symbol, struct Subnode* Sn) {
  unsigned Size = symbol->literal ? 1
       : (Sn == 0) ? 0
       : Sn->Size;
  struct Node* Nd = 0;
  unsigned N = 0;
  for (N = forest->node_pos; N < forest->node_cap; ++N) {
    Nd = &forest->node_table[N];
    if (Nd->symbol == symbol && Nd->Size == Size) break;
  }
  if (N >= forest->node_cap) {
    TABLE_CHECK_GROW(forest->node_table, forest->node_cap, 8, struct Node);
    N = forest->node_cap++;
    Nd = &forest->node_table[N];
    Nd->symbol = symbol;
    Nd->Size = Size;
    Nd->Start = symbol->literal ? forest->position - 1
              : (Sn == 0)   ? forest->position
              : forest->node_table[Sn->Cur].Start;
    Nd->sub_cap = 0;
    Nd->sub_table = 0;
  }
  if (!symbol->literal) {
    unsigned S;
    for (S = 0; S < Nd->sub_cap; ++S) {
      if (subnode_equal(Nd->sub_table[S], Sn)) break;
    }
    if (S >= Nd->sub_cap) {
      TABLE_CHECK_GROW(Nd->sub_table, Nd->sub_cap, 4, struct Subnode*);
      // we are adding a reference to this Subnode, increment its reference count
      // fix by gonzo
      Nd->sub_table[Nd->sub_cap++] = REF(Sn);
    } else {
      subnode_free(Sn);
    }
  }
  return N;
}

static int subnode_equal(struct Subnode* l, struct Subnode* r) {
  for (; l != 0 && r != 0; l = l->next, r = r->next)
    if (l->Size != r->Size || l->Cur != r->Cur) return 0;
  return l == r; // NOTE: why?
}

static unsigned AddQ(Forest* forest, struct ParserState* state) {
  for (unsigned V = forest->vert_pos; V < forest->vert_cap; ++V) {
    struct Vertex* W = &forest->vert_table[V];
    if (W->State == state) return V;
  }
  TABLE_CHECK_GROW(forest->vert_table, forest->vert_cap, 8, struct Vertex);
  struct Vertex* W = &forest->vert_table[forest->vert_cap];
  W->State = state;
  W->Start = forest->position;
  W->Size = 0;
  W->List = 0;
  for (unsigned E = 0; E < state->er_cap; ++E) {
    AddERed(forest, forest->vert_cap, state->er_table[E]);
  }
  return forest->vert_cap++;
}

static void ReduceOne(Forest* forest, struct RRed* rr) {
  struct ZNode* Zn = rr->Zn;
  struct Reduce* Rd = rr->Rd;
  Symbol* L = Rd->lhs;
  RuleSet* rs = &Rd->rs;
  Symbol** R = rs->rules;
  if (forest->fcb && forest->fct) {
    forest->fcb->reduce_rule(forest->fct, rs);
  }

  forest->path_table = 0;
  forest->path_cap = 0;
  unsigned path_index = 0;
  AddLink(forest, Zn, 0);
  for (++R; *R != 0; ++R) {
    // NOTE: AddLink could change the value of forest->path_cap
    for (unsigned path_cap = forest->path_cap; path_index < path_cap; ++path_index) {
      struct Path* path = &forest->path_table[path_index];
      struct Subnode* Sn = path->Sn;
      Zn = path->Zn;
      for (unsigned vertex_pos = 0; vertex_pos < Zn->Size; ++vertex_pos) {
        unsigned vertex_index = Zn->List[vertex_pos];
        struct Vertex* V = &forest->vert_table[vertex_index];
        for (unsigned X = 0; X < V->Size; ++X) {
          AddLink(forest, V->List[X], Sn);
        }
      }
    }
  }

  // NOTE: this makes me think we might want to check for a possibly changing
  // value of forest->path_cap -- is this correct?
  for (; path_index < forest->path_cap; ++path_index) {
    struct Path* path = &forest->path_table[path_index];
    struct Subnode* Sn = path->Sn;
    Zn = path->Zn;
    unsigned N = subnode_add(forest, L, Sn);
    for (unsigned vertex_pos = 0; vertex_pos < Zn->Size; ++vertex_pos) {
      unsigned vertex_index = Zn->List[vertex_pos];
      AddN(forest, N, vertex_index);
    }
  }
  FREE(forest->path_table);
  forest->path_cap = 0;
}

static void AddN(Forest* forest, unsigned N, unsigned vertex_index) {
  struct Node* Nd = &forest->node_table[N];
  struct ParserState* S = NextState(forest, forest->vert_table[vertex_index].State, Nd->symbol);
  if (S == 0) return;
#if 0
  // on my M1 laptop, this does not work...
  struct Vertex* W1 = &forest->vert_table[AddQ(forest, S)];
#else
  // ... but this does -- compiler bug?
  // fix by gonzo
  unsigned pos = AddQ(forest, S);
  struct Vertex* W1 = &forest->vert_table[pos];
#endif

  struct ZNode* Z1 = 0;
  unsigned Z;
  for (Z = 0; Z < W1->Size; ++Z) {
    Z1 = W1->List[Z];
    if (Z1->Index == N) break;
  }
  if (Z >= W1->Size) {
    struct Reduce* Rd;
    unsigned int R;
    TABLE_CHECK_GROW(W1->List, W1->Size, 4, struct ZNode*);
    Z = W1->Size++;
    Z1 = 0;
    MALLOC(struct ZNode, Z1);
    W1->List[Z] = Z1;
    Z1->Index = N;
    Z1->Size = 0;
    Z1->List = 0;
    for (R = 0; R < S->rr_cap; ++R) {
      Rd = &S->rr_table[R];
      // printf("AddRR for ruleset %u\n", Rd->rs.index);
      AddRRed(forest, Z1, Rd);
    }
  }

  unsigned int I;
  for (I = 0; I < Z1->Size; ++I) {
    if (Z1->List[I] == vertex_index) break;
  }
  if (I >= Z1->Size) {
    TABLE_CHECK_GROW(Z1->List, Z1->Size, 4, unsigned);
    I = Z1->Size++;
    Z1->List[I] = vertex_index;
  }
}

static void AddRRed(Forest* forest, struct ZNode* Zn, struct Reduce* Rd) {
  TABLE_CHECK_GROW(forest->rr_table, forest->rr_cap, 8, struct RRed);
  forest->rr_table[forest->rr_cap].Zn = Zn;
  forest->rr_table[forest->rr_cap].Rd = Rd;
  ++forest->rr_cap;
}

static void AddERed(Forest* forest, unsigned vertex_index, Symbol* LHS) {
  TABLE_CHECK_GROW(forest->er_table, forest->er_cap, 8, struct ERed);
  forest->er_table[forest->er_cap].vertex_index = vertex_index;
  forest->er_table[forest->er_cap].LHS = LHS;
  ++forest->er_cap;
}

static void AddLink(Forest* forest, struct ZNode* Zn, struct Subnode* Sn) {
  struct Path* path;
  unsigned N = Zn->Index;
  struct Node* Nd = &forest->node_table[N];
  struct Subnode* NewP = 0;
  MALLOC(struct Subnode, NewP);
  NewP->Size = Nd->Size;
  if (Sn != 0) {
    NewP->Size += Sn->Size;
    REF(Sn);
  }
  NewP->Cur = N;
  NewP->next = Sn;
  NewP->ref_cnt = 0;
  Sn = NewP;
  TABLE_CHECK_GROW(forest->path_table, forest->path_cap, 8, struct Path);
  path = &forest->path_table[forest->path_cap++];
  path->Zn = Zn;
  path->Sn = Sn;
}

static struct ParserState* NextState(Forest* forest, struct ParserState* state, Symbol* symbol) {
  for (unsigned S = 0; S < state->ss_cap; ++S) {
    struct Shift* Sh = &state->ss_table[S];
    if (Sh->symbol == symbol) return &forest->parser->states[Sh->state];
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

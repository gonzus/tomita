#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "mem.h"
#include "lex.h"
#include "sym.h"

#define HASH_MAX 0x100
#define MAX_SYM 0x100

struct Item {
  Symbol LHS;
  Symbol* RHS;
  Symbol* Pos;
};

struct Items {
  Symbol Pre;
  unsigned Size;
  struct Item** List;
};

static Symbol HashTab[HASH_MAX];
static Symbol FirstSym = 0;
static Symbol LastB;
struct State* SList = 0;

static Symbol SymBuf[MAX_SYM];
static Symbol* SymP;

static struct Items* STab;
static unsigned Ss;

static struct Items* XTab;
static unsigned XMax;
static unsigned Xs;

Symbol* FirstB = &FirstSym;

static unsigned char Hash(const char* ptr, unsigned len) {
  int H = 0;
  const char *T;
  for (unsigned pos = 0; pos < len; ++pos) H = (H << 1) ^ ptr[pos];
  return H&0xff;
}

static Symbol sym_lookup_slice(const char *ptr, unsigned len, unsigned char Literal) {
  static int LABEL = 0;
  Symbol Sym; unsigned char H;
  for (H = Hash(ptr, len), Sym = HashTab[H]; Sym != 0; Sym = Sym->Next)
    if (Sym->Literal == Literal && Sym->Name[len] == '\0' && memcmp(Sym->Name, ptr, len) == 0) return Sym;
  Sym = (Symbol)Allocate(sizeof *Sym);
  Sym->Name = CopyB(ptr, len), Sym->Literal = Literal;
  Sym->Index = LABEL++, Sym->Defined = 0;
  Sym->Rules = 0, Sym->RList = 0;
  Sym->Next = HashTab[H], HashTab[H] = Sym;
  Sym->Tail = 0;
  if (FirstSym == 0) FirstSym = Sym; else LastB->Tail = Sym;
  return LastB = Sym;
}

static Symbol sym_lookup(const char *S, unsigned char Literal) {
  return sym_lookup_slice(S, strlen(S), Literal);
}

void LookUpSlice(Symbol* sym, Slice S, unsigned char Literal) {
  *sym = sym_lookup_slice(S.ptr, S.len, Literal);
}

void LookUp(Symbol* sym, const char *S, unsigned char Literal) {
  *sym = sym_lookup(S, Literal);
}

static void InsertR(Symbol S) {
  Rule R; unsigned int I, J, Diff; Symbol *A, *B;
  for (I = 0; I < S->Rules; I++) {
    for (Diff = 0, A = SymBuf, B = S->RList[I]; *A != 0 && *B != 0; A++, B++) {
      Diff = (*A)->Index - (*B)->Index;
      if (Diff != 0) break;
    }
    if (Diff == 0 && *B == 0) {
      if (*A == 0) return; else break;
    } else if (Diff > 0) break;
  }
  if ((S->Rules&7) == 0)
    S->RList = Reallocate(S->RList, sizeof *S->RList*(S->Rules + 8));
  for (J = S->Rules++; J > I; J--) S->RList[J] = S->RList[J - 1];
  S->RList[I] = R = Allocate(sizeof(Symbol) * (SymP - SymBuf));
  for (I = 0; I < (SymP - SymBuf); I++) R[I] = SymBuf[I];
}

void CreateGrammar(Symbol* Start, int* errors) {
  Symbol LHS; Lexical L = LEX(errors); int SawStart = 0;
  *Start = 0;
START:
  switch (L) {
    case EndT: return;
    case IdenT: goto EQUAL;
    case DotT: L = LEX(errors); goto START;
    case ColonT: case EqualT:
      ERROR(LINE, errors, "Missing left-hand side of rule, or '.' from previous rule.");
      goto FLUSH;
    case StarT:
      L = LEX(errors);
      if (L != IdenT) ERROR(LINE, errors, "Missing symbol after '*'.");
      else {
          *Start = sym_lookup(LastW, 0);
          if (SawStart++ > 0) ERROR(LINE, errors, "Start symbol redefined.");
          L = LEX(errors);
        }
      goto FLUSH;
    default: case BarT: ERROR(LINE, errors, "Corrupt rule."); goto FLUSH;
  }
FLUSH:
  for (; L != DotT && L != EndT; L = LEX(errors)) ;
  goto END;
EQUAL:
  LHS = sym_lookup(LastW, 0); if (*Start == 0) *Start = LHS;
  L = LEX(errors); LHS->Defined = 1;
  if (L == DotT) {
    SymP = SymBuf; *SymP++ = LHS, *SymP++ = 0;
    InsertR(sym_lookup(LHS->Name, 1));
  } else if (L == ColonT) {
    SymP = SymBuf; *SymP++ = LHS, *SymP++ = 0;
    for (L = LEX(errors); L == IdenT; L = LEX(errors)) InsertR(sym_lookup(LastW, 1));
  } else if (L == EqualT) {
    do {
      L = LEX(errors);
      for (SymP = SymBuf; L == IdenT && SymP < SymBuf + MAX_SYM; L = LEX(errors))
        *SymP++ = sym_lookup(LastW, 0);
      if (SymP >= SymBuf + MAX_SYM) printf("Large rule.\n"), exit(1);
      *SymP++ = 0; InsertR(LHS);
    } while (L == BarT);
  } else {
    ERROR(LINE, errors, "Missing '=' or ':'."); goto FLUSH;
  }
END:
  if (L == EndT) { ERROR(LINE, errors, "Missing '.'"); return; }
  if (L == DotT) L = LEX(errors);
  goto START;
}

void Check(int* errors) {
  Symbol S;
  for (S = FirstSym; S != 0; S = S->Tail)
    if (!S->Defined && !S->Literal) ERROR(LINE, errors, "%s undefined.\n", S->Name);
  if (*errors > 0) printf("Aborted.\n"), exit(1);
}

struct Item* CopyI(struct Item* A) {
  struct Item* B = Allocate(sizeof *B);
  *B = *A;
  return B;
}

int CompI(struct Item* A, struct Item* B) {
  int Diff; Symbol *AP, *BP;
  Diff =
    A->LHS == 0? (B->LHS == 0? 0: -1):
    B->LHS == 0? +1: A->LHS->Index - B->LHS->Index;
  if (Diff != 0) return Diff;
  Diff = (A->Pos - A->RHS) - (B->Pos - B->RHS); if (Diff != 0) return Diff;
  for (AP = A->RHS, BP = B->RHS; *AP != 0 && *BP != 0; AP++, BP++)
    if ((Diff = (*AP)->Index - (*BP)->Index) != 0) break;
  return *AP == 0? (*BP == 0? 0: -1): *BP == 0? +1: Diff;
}

int AddState(unsigned int Size, struct Item* *List) {
  unsigned int I, S; struct Items* IS;
  for (S = 0; S < Ss; S++) {
    IS = &STab[S];
    if (IS->Size != Size) continue;
    for (I = 0; I < IS->Size; I++)
      if (CompI(IS->List[I], List[I]) != 0) break;
    if (I >= IS->Size) { FREE(List); return S; }
  }
  if ((Ss&7) == 0)
    STab = Reallocate(STab, (Ss + 8) * sizeof *STab),
      SList = Reallocate(SList, (Ss + 8) * sizeof *SList);
  STab[Ss].Pre = 0, STab[Ss].Size = Size, STab[Ss].List = List;
  return Ss++;
}

struct Items* GetItem(Symbol Pre) {
  unsigned X;
  for (X = 0; X < Xs; X++)
    if (Pre == XTab[X].Pre) break;
  if (X >= Xs) {
    if (Xs >= XMax) XMax += 8, XTab = Reallocate(XTab, XMax * sizeof *XTab);
    X = Xs++;
    XTab[X].Pre = Pre, XTab[X].Size = 0, XTab[X].List = 0;
  }
  return &XTab[X];
}

struct Item* FormItem(Symbol LHS, Rule RHS) {
  struct Item* It = Allocate(sizeof *It);
  It->LHS = LHS, It->Pos = It->RHS = RHS;
  return It;
}

void AddItem(struct Items* Q, struct Item* It) {
  unsigned int I, J, Diff;
  for (I = 0; I < Q->Size; I++) {
    Diff = CompI(Q->List[I], It);
    if (Diff == 0) return;
    if (Diff > 0) break;
  }
  if ((Q->Size&3) == 0)
    Q->List = Reallocate(Q->List, (Q->Size + 4) * sizeof(struct Item*));
  for (J = Q->Size++; J > I; J--) Q->List[J] = Q->List[J - 1];
  Q->List[I] = CopyI(It);
}

void Generate(Symbol* Start) {
  struct Item* It;
  struct Item* *Its;
  struct Item* *QBuf;
  unsigned int S, X, Q, Qs, QMax; struct Items* QS;
  Rule StartR = Allocate(2 * sizeof(Symbol));
  StartR[0] = *Start, StartR[1] = 0;
  Its = Allocate(sizeof(struct Item*)), Its[0] = FormItem(0, StartR);
  SList = 0, STab = 0, Ss = 0; AddState(1, Its);
  XTab = 0, XMax = 0;
  QBuf = 0, QMax = 0;
  for (S = 0; S < Ss; S++) {
    unsigned ERs, RRs, E, R; unsigned char Final;
    QS = &STab[S];
    if (QS->Size > QMax)
      QMax = QS->Size, QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
    for (Qs = 0; Qs < QS->Size; Qs++)
      QBuf[Qs] = QS->List[Qs];
    for (ERs = RRs = 0, Final = 0, Xs = 0, Q = 0; Q < Qs; Q++) {
      It = QBuf[Q];
      if (*It->Pos == 0) {
        if (It->LHS == 0) Final++;
        else if (*It->RHS == 0) ERs++; else RRs++;
      } else {
        Symbol Pre = *It->Pos++; struct Items* IS = GetItem(Pre);
        if (IS->Size == 0) {
          if (Qs + Pre->Rules > QMax)
            QMax = Qs + Pre->Rules,
              QBuf = Reallocate(QBuf, QMax * sizeof(struct Item*));
          for (R = 0; R < Pre->Rules; R++, Qs++)
            QBuf[Qs] = FormItem(Pre, Pre->RList[R]);
        }
        AddItem(IS, It);
        --It->Pos;
      }
    }
    MakeState(&SList[S], Final, ERs, RRs, Xs);
    for (E = R = 0, Q = 0; Q < Qs; Q++) {
      It = QBuf[Q];
      if (*It->Pos != 0 || It->LHS == 0) continue;
      if (*It->RHS == 0) SList[S].EList[E++] = It->LHS;
      else {
          struct Reduce* Rd = &SList[S].RList[R++];
          Rd->LHS = It->LHS, Rd->RHS = It->RHS;
        }
    }
    for (X = 0; X < Xs; X++) {
      struct Shift* Sh = &SList[S].SList[X];
      Sh->X = XTab[X].Pre, Sh->Q = AddState(XTab[X].Size, XTab[X].List);
    }
  }
  FREE(XTab);
  FREE(QBuf);
}

void SHOW_STATES(void) {
  unsigned S; struct State* St; Rule R; unsigned int I;
  for (S = 0; S < Ss; S++) {
    St = &SList[S];
    printf("%d:\n", S);
    if (St->Final) printf("\taccept\n");
    for (I = 0; I < St->Es; I++)
      printf("\t[%s -> 1]\n", St->EList[I]->Name);
    for (I = 0; I < St->Rs; I++) {
      printf("\t[%s ->", St->RList[I].LHS->Name);
      for (R = St->RList[I].RHS; *R != 0; R++) printf(" %s", (*R)->Name);
      printf("]\n");
    }
    for (I = 0; I < St->Ss; I++)
      printf("\t%s => %d\n", St->SList[I].X->Name, St->SList[I].Q);
  }
}

struct State* Next(struct State* Q, Symbol* Sym) {
  unsigned int S; struct Shift* Sh;
  for (S = 0; S < Q->Ss; S++) {
    Sh = &Q->SList[S];
    if (Sh->X == *Sym) return &SList[Sh->Q];
  }
  return 0;
}

void MakeState(struct State* S, unsigned char Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss) {
  S->Final = Final;
  S->Es = new_Es, S->EList = new_Es == 0? 0: Allocate(new_Es * sizeof(Symbol));
  S->Rs = new_Rs, S->RList = new_Rs == 0? 0: Allocate(new_Rs * sizeof(struct Reduce));
  S->Ss = new_Ss, S->SList = new_Ss == 0? 0: Allocate(new_Ss * sizeof(struct Shift));
}

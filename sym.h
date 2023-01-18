#pragma once

typedef unsigned char byte;
typedef struct SymbolData *Symbol;
typedef struct SymbolData **Rule;

struct SymbolData {
  char *Name;
  byte Defined;
  byte Literal;
  unsigned Index;
  unsigned Rules;
  Rule* RList;
  struct SymbolData *Next;
  struct SymbolData *Tail;
};

struct Reduce {
  Symbol LHS;
  Symbol* RHS;
};

struct Shift {
  Symbol X;
  int Q;
};

struct State {
  byte Final;
  unsigned Es;
  unsigned Rs;
  unsigned Ss;
  Symbol* EList;
  struct Reduce* RList;
  struct Shift* SList;
};

extern Symbol FirstB;
extern struct State* SList;

Symbol Grammar(int* errors);

void Check(int* errors);

void Generate(Symbol Start);

void SHOW_STATES(void);

Symbol LookUp(char *S, byte Literal);

struct State* Next(struct State* Q, Symbol Sym);

void MakeState(struct State* S, byte Final, unsigned new_Es, unsigned new_Rs, unsigned new_Ss);

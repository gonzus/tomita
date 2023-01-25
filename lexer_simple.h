#pragma once

#include "lexer.h"
#include "symtab.h" // TODO: surely this is a bad idea?

typedef struct Lexer {
  Slice input;
  unsigned pos;
  SymTab* symtab;
} Lexer;

Lexer* lexer_create(SymTab* symtab);
void lexer_destroy(Lexer* lexer);

void lexer_set_input(Lexer* lexer, Slice input);
Symbol* lexer_next_token(Lexer* lexer);

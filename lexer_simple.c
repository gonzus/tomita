#include <ctype.h>
#include "mem.h"
#include "lexer_simple.h"

LexerI data = {
  .next_token = (LexerNextTokenFn*) lexer_next_token,
  .set_input = (LexerSetInputFn*) lexer_set_input,
  .context = 0,
};

LexerI* lexer_data(void* context) {
  data.context = context;
  return &data;
}

Lexer* lexer_create(SymTab* symtab) {
  Lexer* lexer = 0;
  MALLOC(Lexer, lexer);
  lexer->symtab = symtab;
  return lexer;
}

void lexer_destroy(Lexer* lexer) {
  FREE(lexer);
}

void lexer_set_input(Lexer* lexer, Slice input) {
  lexer->input = input;
  lexer->pos = 0;
}

Symbol* lexer_next_token(Lexer* lexer) {
  Symbol* sym = 0;
  do {
    // skip whitespace
    while (lexer->pos < lexer->input.len &&
           (lexer->input.ptr[lexer->pos] == ' ' || lexer->input.ptr[lexer->pos] == '\t')) {
      ++lexer->pos;
    }

    // ran out? done
    if (lexer->pos >= lexer->input.len) break;

    // work line by line -- good for an interactive session
    if (lexer->input.ptr[lexer->pos] == '\n') {
      ++lexer->pos;
      break;
    }

    // gather non-space characters as symbol name
    unsigned beg = lexer->pos;
    while (1) {
      if (lexer->pos >= lexer->input.len) break;
      if (isspace(lexer->input.ptr[lexer->pos])) break;
      ++lexer->pos;
    }
    Slice name = slice_from_memory(lexer->input.ptr + beg, lexer->pos - beg);
    symtab_lookup(lexer->symtab, name, 1, &sym);
  } while (0);

  return sym;
}

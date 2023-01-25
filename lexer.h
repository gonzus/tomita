#pragma once

#include "slice.h"
#include "symbol.h" // TODO: remove this and create a Token interface

typedef void (LexerSetInputFn)(void* context, Slice input);
typedef Symbol* (LexerNextTokenFn)(void* context);

typedef struct LexerI {
  void* context;
  LexerSetInputFn* set_input;
  LexerNextTokenFn* next_token;
} LexerI;

// This has to be implemented in the concrete lexer.
LexerI* lexer_data(void* context);

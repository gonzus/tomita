#pragma once

#include "buffer.h"
#include "forest.h"

enum TomitaFormat {
  FORMAT_COMMENT     = '#',
  FORMAT_GRAMMAR     = 'G',
  FORMAT_SYMTAB      = 'S',
  FORMAT_SYMBOL      = 'y',
  FORMAT_RULE        = 'u',
  FORMAT_PARSER      = 'P',
  FORMAT_STATE       = 'T',
  FORMAT_SHIFT       = 's',
  FORMAT_REDUCE      = 'r',
  FORMAT_EPSILON     = 'e',
};

enum TomitaGrammar {
  GRAMMAR_TERMINATOR = ';',
  GRAMMAR_EQ_RULE    = ':',
  GRAMMAR_EQ_TOKEN   = '=',
  GRAMMAR_START      = '@',
  GRAMMAR_OR         = '|',
  GRAMMAR_SLASH      = '/',
  GRAMMAR_LT         = '<',
  GRAMMAR_MINUS      = '-',
};

// Tomita is the boss
typedef struct Tomita {
  struct SymTab* symtab;
  struct Grammar* grammar;
  struct Parser* parser;
  struct Forest* forest;
  ForestCallbacks* cb;
  void* ctx;
} Tomita;

// Create an empty Tomita.
Tomita* tomita_create(ForestCallbacks* cb, void* ctx);

// Destroy a Tomita created with tomita_create().
void tomita_destroy(Tomita* tomita);

// Clear all contents of a Tomita.
void tomita_clear(Tomita* tomita);

// Show the contents of a Tomita.
unsigned tomita_show(Tomita* tomita);

// grammar functions
unsigned tomita_grammar_show(Tomita* tomita);
unsigned tomita_grammar_compile_from_slice(Tomita* tomita, Slice grammar);
unsigned tomita_grammar_read_from_slice(Tomita* tomita, Slice grammar);
unsigned tomita_grammar_write_to_buffer(Tomita* tomita, Buffer* b);

// parser functions
unsigned tomita_parser_show(Tomita* tomita);
unsigned tomita_parser_build_from_grammar(Tomita* tomita);
unsigned tomita_parser_read_from_slice(Tomita* tomita, Slice parser);
unsigned tomita_parser_write_to_buffer(Tomita* tomita, Buffer* b);

// forest functions
unsigned tomita_forest_show(Tomita* tomita);
unsigned tomita_forest_parse_from_slice(Tomita* tomita, Slice source);

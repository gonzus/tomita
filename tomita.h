#pragma once

#include "symtab.h"
#include "grammar.h"
#include "parser.h"
#include "forest.h"

#define FORMAT_COMMENT     '#'
#define FORMAT_GRAMMAR     'G'
#define FORMAT_SYMTAB      'S'
#define FORMAT_SYMBOL      'y'
#define FORMAT_RULE        'u'
#define FORMAT_PARSER      'P'
#define FORMAT_STATE       'T'
#define FORMAT_SHIFT       's'
#define FORMAT_REDUCE      'r'
#define FORMAT_EPSILON     'e'

#define GRAMMAR_TERMINATOR ';'
#define GRAMMAR_EQ_RULE    ':'
#define GRAMMAR_EQ_TOKEN   '='
#define GRAMMAR_START      '@'
#define GRAMMAR_OR         '|'
#define GRAMMAR_SLASH      '/'
#define GRAMMAR_LT         '<'
#define GRAMMAR_MINUS      '-'

// Tomita is the boss
typedef struct Tomita {
  SymTab* symtab;
  Grammar* grammar;
  Parser* parser;
  Forest* forest;
} Tomita;

// Create an empty Tomita.
Tomita* tomita_create(void);

// Destroy a Tomita created with tomita_create().
void tomita_destroy(Tomita* tomita);

// Clear all contents of a Tomita.
void tomita_clear(Tomita* tomita);

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

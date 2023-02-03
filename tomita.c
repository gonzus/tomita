#include "mem.h"
#include "log.h"
#include "tomita.h"

static void clear_forest(Tomita* tomita);
static void clear_parser(Tomita* tomita);
static void clear_grammar(Tomita* tomita);
static void clear_symtab(Tomita* tomita);

static void reset_forest(Tomita* tomita);
static void reset_parser(Tomita* tomita);
static void reset_grammar(Tomita* tomita);
static void reset_symtab(Tomita* tomita);

Tomita* tomita_create(void) {
  Tomita* tomita = 0;
  MALLOC(Tomita, tomita);
  return tomita;
}

void tomita_destroy(Tomita* tomita) {
  tomita_clear(tomita);
  FREE(tomita);
}

void tomita_clear(Tomita* tomita) {
  clear_forest(tomita);
  clear_parser(tomita);
  clear_grammar(tomita);
  clear_symtab(tomita);
}

unsigned tomita_show_grammar(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->grammar) {
      LOG_WARN("tomita: cannot show null grammar");
      ++errors;
      break;
    }
    grammar_show(tomita->grammar);
  } while (0);
  return errors;
}

unsigned tomita_compile_grammar_from_slice(Tomita* tomita, Slice grammar) {
  unsigned errors = 0;
  do {
    reset_grammar(tomita);
    errors += grammar_compile_from_slice(tomita->grammar, grammar);
  } while (0);
  return errors;
}

unsigned tomita_read_grammar_from_slice(Tomita* tomita, Slice grammar) {
  unsigned errors = 0;
  do {
    reset_grammar(tomita);
    // TODO: make names uniform
    errors += grammar_load_from_slice(tomita->grammar, grammar);
  } while (0);
  return errors;
}

unsigned tomita_write_grammar_to_buffer(Tomita* tomita, Buffer* b) {
  unsigned errors = 0;
  do {
    if (!tomita->grammar) {
      LOG_WARN("tomita: cannot save null grammar");
      ++errors;
      break;
    }
  // TODO: make names uniform
    errors += grammar_save_to_buffer(tomita->grammar, b);
  } while (0);
  return errors;
}

unsigned tomita_show_parser(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->parser) {
      LOG_WARN("tomita: cannot show null parser");
      ++errors;
      break;
    }
    parser_show(tomita->parser);
  } while (0);
  return errors;
}

unsigned tomita_build_parser_from_grammar(Tomita* tomita) {
  unsigned errors = 0;
  do {
    reset_parser(tomita);
    if (!tomita->grammar) {
      LOG_WARN("tomita: cannot build parser from null grammar");
      ++errors;
      break;
    }
    errors += parser_build_from_grammar(tomita->parser, tomita->grammar);
  } while (0);
  return errors;
}

unsigned tomita_read_parser_from_slice(Tomita* tomita, Slice parser) {
  unsigned errors = 0;
  do {
    reset_symtab(tomita);
    // TODO: make names uniform
    errors += parser_load_from_slice(tomita->parser, parser);
  } while (0);
  return errors;
}

unsigned tomita_write_parser_to_buffer(Tomita* tomita, Buffer* b) {
  unsigned errors = 0;
  do {
    if (!tomita->parser) {
      LOG_WARN("tomita: cannot save null parser");
      ++errors;
      break;
    }
  // TODO: make names uniform
    errors += parser_save_to_buffer(tomita->parser, b);
  } while (0);
  return errors;
}

unsigned tomita_show_forest(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->forest) {
      LOG_WARN("tomita: cannot show null forest");
      ++errors;
      break;
    }
    forest_show(tomita->forest);
  } while (0);
  return errors;
}

struct Node* tomita_parse_slice_into_forest(Tomita* tomita, Slice source) {
  struct Node* node = 0;
  unsigned errors = 0;
  do {
    reset_forest(tomita);
    if (!tomita->parser) {
      LOG_WARN("tomita: cannot parse slice with null parser");
      ++errors;
      break;
    }
    node = forest_parse(tomita->forest, source);
  } while (0);
  return node;
}

static void clear_forest(Tomita* tomita) {
  if (!tomita->forest) return;

  LOG_INFO("tomita: clearing forest");
  forest_destroy(tomita->forest);
  tomita->forest = 0;
}

static void clear_parser(Tomita* tomita) {
  if (!tomita->parser) return;

  LOG_INFO("tomita: clearing parser");
  parser_destroy(tomita->parser);
  tomita->parser = 0;
}

static void clear_grammar(Tomita* tomita) {
  if (!tomita->grammar) return;

  LOG_INFO("tomita: clearing grammar");
  grammar_destroy(tomita->grammar);
  tomita->grammar = 0;
}

static void clear_symtab(Tomita* tomita) {
  if (!tomita->symtab) return;

  LOG_INFO("tomita: clearing symtab");
  symtab_destroy(tomita->symtab);
  tomita->symtab = 0;
}

static void reset_forest(Tomita* tomita) {
  clear_forest(tomita);

  LOG_INFO("tomita: creating forest");
  tomita->forest = forest_create(tomita->parser);
}

static void reset_parser(Tomita* tomita) {
  clear_forest(tomita);
  clear_parser(tomita);

  LOG_INFO("tomita: creating parser");
  tomita->parser = parser_create(tomita->symtab);
}

static void reset_grammar(Tomita* tomita) {
  clear_forest(tomita);
  clear_parser(tomita);
  clear_grammar(tomita);
  clear_symtab(tomita);

  LOG_INFO("tomita: creating symtab");
  tomita->symtab = symtab_create();
  LOG_INFO("tomita: creating grammar");
  tomita->grammar = grammar_create(tomita->symtab);
}

static void reset_symtab(Tomita* tomita) {
  clear_forest(tomita);
  clear_parser(tomita);
  clear_grammar(tomita);
  clear_symtab(tomita);

  LOG_INFO("tomita: creating symtab");
  tomita->symtab = symtab_create();
}

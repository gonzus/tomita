#include "mem.h"
#include "log.h"
#include "grammar.h"
#include "parser.h"
#include "symtab.h"
#include "forest.h"
#include "tomita.h"

static void ensure_forest(Tomita* tomita);
static void ensure_parser(Tomita* tomita);
static void ensure_grammar(Tomita* tomita);
static void ensure_symtab(Tomita* tomita);

Tomita* tomita_create(ForestCallbacks* cb, void* ctx) {
  Tomita* tomita = 0;
  MALLOC(Tomita, tomita);
  tomita->cb = cb;
  tomita->ctx = ctx;
  return tomita;
}

void tomita_destroy(Tomita* tomita) {
  if (!tomita) return;
  if (tomita->forest) {
    forest_destroy(tomita->forest);
    tomita->forest = 0;
  }
  if (tomita->parser) {
    parser_destroy(tomita->parser);
    tomita->parser = 0;
  }
  if (tomita->grammar) {
    grammar_destroy(tomita->grammar);
    tomita->grammar = 0;
  }
  if (tomita->symtab) {
    symtab_destroy(tomita->symtab);
    tomita->symtab = 0;
  }
  FREE(tomita);
}

void tomita_clear(Tomita* tomita) {
  if (!tomita) return;
  if (tomita->forest) {
    forest_clear(tomita->forest);
  }
  if (tomita->parser) {
    parser_clear(tomita->parser);
  }
  if (tomita->grammar) {
    grammar_clear(tomita->grammar);
  }
  if (tomita->symtab) {
    symtab_clear(tomita->symtab);
  }
}

unsigned tomita_grammar_show(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->grammar) {
      LOG_DEBUG("tomita: cannot show null grammar");
      ++errors;
      break;
    }
    grammar_show(tomita->grammar);
  } while (0);
  return errors;
}

unsigned tomita_grammar_compile_from_slice(Tomita* tomita, Slice grammar) {
  unsigned errors = 0;
  do {
    ensure_grammar(tomita);
    errors += grammar_compile_from_slice(tomita->grammar, grammar);
  } while (0);
  return errors;
}

unsigned tomita_grammar_read_from_slice(Tomita* tomita, Slice grammar) {
  unsigned errors = 0;
  do {
    ensure_grammar(tomita);
    errors += grammar_load_from_slice(tomita->grammar, grammar);
  } while (0);
  return errors;
}

unsigned tomita_grammar_write_to_buffer(Tomita* tomita, Buffer* b) {
  unsigned errors = 0;
  do {
    if (!b) {
      ++errors;
      break;
    }
    ensure_grammar(tomita);
    errors += grammar_save_to_buffer(tomita->grammar, b);
  } while (0);
  return errors;
}

unsigned tomita_parser_show(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->parser) {
      LOG_DEBUG("tomita: cannot show null parser");
      ++errors;
      break;
    }
    parser_show(tomita->parser);
  } while (0);
  return errors;
}

unsigned tomita_parser_build_from_grammar(Tomita* tomita) {
  unsigned errors = 0;
  do {
    ensure_parser(tomita);
    ensure_grammar(tomita);
    errors += parser_build_from_grammar(tomita->parser, tomita->grammar);
  } while (0);
  return errors;
}

unsigned tomita_parser_read_from_slice(Tomita* tomita, Slice parser) {
  unsigned errors = 0;
  do {
    ensure_parser(tomita);
    errors += parser_load_from_slice(tomita->parser, parser);
  } while (0);
  return errors;
}

unsigned tomita_parser_write_to_buffer(Tomita* tomita, Buffer* b) {
  unsigned errors = 0;
  do {
    if (!b) {
      ++errors;
      break;
    }
    ensure_parser(tomita);
    errors += parser_save_to_buffer(tomita->parser, b);
  } while (0);
  return errors;
}

unsigned tomita_forest_show(Tomita* tomita) {
  unsigned errors = 0;
  do {
    if (!tomita->forest) {
      LOG_DEBUG("tomita: cannot show null forest");
      ++errors;
      break;
    }
    forest_show(tomita->forest);
  } while (0);
  return errors;
}

unsigned tomita_show(Tomita* tomita) {
  unsigned errors = 0;
  errors += tomita_grammar_show(tomita);
  errors += tomita_parser_show(tomita);
  errors += tomita_forest_show(tomita);
  return errors;
}

unsigned tomita_forest_parse_from_slice(Tomita* tomita, Slice source) {
  unsigned errors = 0;
  do {
    ensure_forest(tomita);
    errors = forest_parse(tomita->forest, source);
    if (!tomita->forest || !tomita->forest->root) {
      ++errors;
      break;
    }
  } while (0);
  return errors;
}

static void ensure_forest(Tomita* tomita) {
  if (!tomita) return;
  ensure_parser(tomita);
  if (tomita->forest) return;
  tomita->forest = forest_create(tomita->parser, tomita->cb, tomita->ctx);
}

static void ensure_parser(Tomita* tomita) {
  if (!tomita) return;
  ensure_symtab(tomita);
  if (tomita->parser) return;
  tomita->parser = parser_create(tomita->symtab);
}

static void ensure_grammar(Tomita* tomita) {
  if (!tomita) return;
  ensure_symtab(tomita);
  if (tomita->grammar) return;
  tomita->grammar = grammar_create(tomita->symtab);
}

static void ensure_symtab(Tomita* tomita) {
  if (!tomita) return;
  if (tomita->symtab) return;
  tomita->symtab = symtab_create();
}

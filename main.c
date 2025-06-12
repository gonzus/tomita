#include <stdio.h>
#include <unistd.h>
#include "log.h"
#include "timer.h"
#include "util.h"
#include "tomita.h"

#define MAX_BUF (1024*1024)

static char* opt_grammar_file = 0;
static int opt_grammar = 0;
static int opt_stack = 0;
static int opt_table = 0;

static unsigned process_line(Tomita* tomita, Slice line) {
  unsigned errors = 0;
  LOG_INFO("parsing line [%.*s]", line.len, line.ptr);
  do {
    errors = tomita_forest_parse_from_slice(tomita, line);
    if (errors) break;
    LOG_INFO("parsed input, got forest:");
    forest_show(tomita->forest);

    if (opt_stack) {
      LOG_INFO("parse stack:");
      forest_show_stack(tomita->forest);
    }
  } while (0);
  return errors;
}

static unsigned process_path(Tomita* tomita, const char* path) {
  FILE* fp = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) {
      LOG_WARN("could not open [%s] for reading", path);
      break;
    }

    while (1) {
      char buf[MAX_BUF];
      if (!fgets(buf, MAX_BUF, fp)) break;
      Slice raw = slice_from_string(buf, 0);
      Slice line = slice_trim(raw);
      process_line(tomita, line);
    }
  } while (0);
  if (fp)
    fclose(fp);
  return 0;
}

static void show_usage(const char* prog) {
  printf(
      "Usage: %s -f file [-gts] file ...\n"
      "   -f  use this grammar file (required)\n"
      "   -g  display input grammar\n"
      "   -t  display parsing table\n"
      "   -s  display parsing stack\n"
      "   -?  print this help\n",
      prog
    );
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "gtsf:?")) != -1) {
    switch (c) {
      case 'g':
        opt_grammar = 1;
        break;
      case 't':
        opt_table = 1;
        break;
      case 's':
        opt_stack = 1;
        break;
      case 'f':
        opt_grammar_file = optarg;
        break;
      case '?':
      default:
        show_usage(argv[0]);
        return 0;
    }
  }
  if (!opt_grammar_file) {
    show_usage(argv[0]);
    return 0;
  }
  argc -= optind;
  argv += optind;

  Tomita* tomita = 0;
  Buffer data; buffer_build(&data);
  do {
    unsigned errors = 0;
    Timer timer;

    buffer_clear(&data);
    timer_start(&timer);
    file_slurp(opt_grammar_file, &data);
    timer_stop(&timer);
    if (data.len == 0) {
      LOG_INFO("could not read grammar file [%s]", opt_grammar_file);
      break;
    };
    LOG_INFO("read %u bytes from grammar file [%s] in %luus", data.len, opt_grammar_file, timer_elapsed_us(&timer));
    LOG_INFO("\n%.*s", data.len, data.ptr);

    timer_start(&timer);
    tomita = tomita_create();
    timer_stop(&timer);
    if (!tomita) {
      LOG_INFO("could not create tomita");
      break;
    };
    LOG_INFO("tomita created in %luus", timer_elapsed_us(&timer));

    Slice raw = buffer_slice(&data);
    Slice text_grammar = slice_trim(raw);
    timer_start(&timer);
    errors = tomita_grammar_compile_from_slice(tomita, text_grammar);
    timer_stop(&timer);
    if (errors) break;
    LOG_INFO("compiled grammar from source in %luus", timer_elapsed_us(&timer));

    if (opt_grammar) tomita_grammar_show(tomita);

    timer_start(&timer);
    errors = tomita_parser_build_from_grammar(tomita);
    timer_stop(&timer);
    if (errors) break;
    LOG_INFO("built parser from grammar in %luus", timer_elapsed_us(&timer));

    if (opt_table) tomita_parser_show(tomita);

    for (int j = 0; j < argc; ++j) {
      errors = process_path(tomita, argv[j]);
      if (errors) break;
    }
  } while (0);
  buffer_destroy(&data);
  if (tomita) tomita_destroy(tomita);

  return 0;
}

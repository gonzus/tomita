#include <stdio.h>
#include <unistd.h>
#include "log.h"
#include "buffer.h"
#include "timer.h"
#include "util.h"
#include "symbol.h"
#include "forest.h"
#include "tomita.h"

#define MAX_BUF (1024*1024)

#if 1
#define PUSH(c, v) do { (c)->stack[(c)->spos++] = v; if ((c)->spos >= 100) printf("FUCK PUSH\n"); } while (0)
#define POP(c) ((c)->spos == 0 ? printf("FUCK POP\n") : (c)->stack[--(c)->spos])
#define CHECK(c, l) assert((c)->spos >= (l))
#else
#define PUSH(c, v) do { } while (0)
#define POP(c) 0
#define CHECK(c, l) 0
#endif

typedef struct Context {
  int stack[100];
  int spos;
} Context;

static int new_token(void* fct, Slice t) {
  Context* context = (Context*) fct;
  PUSH(context, t.ptr[0]);
  printf("new_token: [%c] (%.*s)\n", t.ptr[0], t.len, t.ptr);
  return 0;
}

static int reduce_rule(void* fct, RuleSet* rs) {
  Context* context = (Context*) fct;
  int s = context->spos;
  switch (rs->index) {
    case 0:
      CHECK(context, 1);
      break;
    case 1: {
      CHECK(context, 3);
      int t = POP(context);
      POP(context); // -
      int e = POP(context);
      PUSH(context, e - t);
      break;
    }
    case 2: {
      CHECK(context, 3);
      int t = POP(context);
      POP(context); // +
      int e = POP(context);
      PUSH(context, e + t);
      break;
    }
    case 3:
      CHECK(context, 1);
      break;
    case 4: {
      CHECK(context, 3);
      int f = POP(context);
      POP(context); // *
      int t = POP(context);
      PUSH(context, t * f);
      break;
    }
    case 5: {
      CHECK(context, 3);
      int f = POP(context);
      POP(context); // /
      int t = POP(context);
      PUSH(context, t / f);
      break;
    }
    case 6: {
      CHECK(context, 1);
      int d = POP(context);
      PUSH(context, d - '0');
      break;
    }
    case 7: {
      CHECK(context, 3);
      POP(context); // )
      int e = POP(context);
      POP(context); // (
      PUSH(context, e);
      break;
    }
    case 8: {
      CHECK(context, 2);
      int f = POP(context);
      POP(context); // -
      PUSH(context, -f);
      break;
    }
    default:
      printf("WTF?!?\n");
      break;
  }
  printf("reduce rule %u; %u -> %u\n", rs->index, s, context->spos);
  return 0;
}

static int accept(void* fct) {
  Context* context = (Context*) fct;
  int v = POP(context);
  printf("Final value: %d\n", v);
  printf("Final stack pos: %d\n", context->spos);
  return 0;
}

static char* opt_grammar_file = 0;
static int opt_grammar = 0;
static int opt_stack = 0;
static int opt_stdin = 0;
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

static unsigned process_file(Tomita* tomita, FILE* fp) {
  while (fp) {
    char buf[MAX_BUF];
    if (!fgets(buf, MAX_BUF, fp)) break;
    Slice raw = slice_from_string(buf, 0);
    Slice line = slice_trim(raw);
    process_line(tomita, line);
  }
  return 0;
}

static void show_usage(const char* prog) {
  printf(
      "Usage: %s -f file [-gts] file ...\n"
      "   -f      use this grammar file (required)\n"
      "   -g      display input grammar\n"
      "   -t      display parsing table\n"
      "   -s      display parsing stack\n"
      "   -n      use stdin for input\n"
      "   -h, -?  print this help\n",
      prog
    );
}

int main(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "gtsnf:h?")) != -1) {
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
      case 'n':
        opt_stdin = 1;
        break;
      case 'f':
        opt_grammar_file = optarg;
        break;
      case 'h':
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

    ForestCallbacks cb = {
      new_token,
      reduce_rule,
      accept,
    };
    Context context = {
      .spos = 0,
    };
    timer_start(&timer);
    tomita = tomita_create(&cb, &context);
    // tomita = tomita_create(0, 0);
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

    if (opt_stdin) {
      errors = process_file(tomita, stdin);
    } else {
      for (int j = 0; j < argc; ++j) {
        LOG_INFO("ARG %u [%s]", j, argv[j]);
        FILE* fp = fopen(argv[j], "r");
        errors = process_file(tomita, fp);
        fclose(fp);
        if (errors) break;
      }
    }
  } while (0);
  buffer_destroy(&data);
  if (tomita) tomita_destroy(tomita);

  return 0;
}

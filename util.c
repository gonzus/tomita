#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "util.h"

#define MAX_ERRORS 25

void ERROR(int line, int* errors, const char *Format, ...) {
  va_list AP;
  fprintf(stderr, "[%d] ", line);
  va_start(AP, Format);
  vfprintf(stderr, Format, AP);
  va_end(AP);
  fputc('\n', stderr);
  if (++*errors == MAX_ERRORS) {
    fprintf(stderr, "Reached the %d error limit.\n", MAX_ERRORS);
    exit(1);
  }
}

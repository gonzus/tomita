#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "util.h"

#define MAX_ERRORS 25


unsigned slurp_file(const char* path, char* buf, unsigned cap) {
  unsigned len = 0;
  FILE* fp = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) break;

    struct stat st = {0};
    if (fstat(fileno(fp), &st)) break;
    len = st.st_size;
    if (len >= cap) {
      len = 0;
      break;
    }
    unsigned nr = fread(buf, 1, len, fp);
    if (nr != len) {
      len = 0;
      break;
    }
  } while (0);
  if (fp) {
    fclose(fp);
  }
  return len;
}

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

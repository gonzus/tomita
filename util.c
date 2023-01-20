#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/stat.h>
#include "util.h"

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

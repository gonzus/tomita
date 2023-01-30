#include <sys/stat.h>
#include "util.h"

unsigned slurp_file(const char* path, Buffer* b) {
  FILE* fp = 0;
  unsigned len = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) break;

    len = slurp_stream(fp, b);
  } while (0);
  if (fp) {
    fclose(fp);
  }
  b->len = len;
  return len;
}

unsigned slurp_stream(FILE* fp, Buffer* b) {
  unsigned len = 0;
  do {
    struct stat st = {0};
    if (fstat(fileno(fp), &st)) break;
    len = st.st_size;
    buffer_ensure_total(b, len);
    unsigned nr = fread(b->ptr, 1, len, fp);
    if (nr != len) {
      len = 0;
      break;
    }
  } while (0);
  b->len = len;
  return len;
}

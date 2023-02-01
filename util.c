#include <ctype.h>
#include <sys/stat.h>
#include "log.h"
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

unsigned skip_spaces(Slice line, unsigned pos) {
  while (pos < line.len && isspace(line.ptr[pos])) ++pos;
  return pos;
}

unsigned next_number(Slice line, unsigned pos, unsigned* number) {
  *number = 0;
  pos = skip_spaces(line, pos);
  unsigned digits = 0;
  while (pos < line.len && isdigit(line.ptr[pos])) {
    *number *= 10;
    *number += line.ptr[pos] - '0';
    ++pos;
    ++digits;
  }
  LOG_DEBUG("next number=%u, digits=%u, pos=%u", *number, digits, pos);
  return digits > 0 ? pos : 0;
}

unsigned next_string(Slice line, unsigned pos, Slice* string) {
  string->ptr = 0;
  string->len = 0;
  pos = skip_spaces(line, pos);
  while (pos < line.len) {
    if (line.ptr[pos] == '[') {
      ++pos;
      if (pos < line.len) string->ptr = &line.ptr[pos];
      continue;
    }
    if (line.ptr[pos] == ']') {
      string->len = &line.ptr[pos] - string->ptr;
      ++pos;
      break;
    }
    ++pos;
  }
  LOG_DEBUG("next string=<%.*s>, pos=%u", string->len, string->ptr, pos);
  return pos;
}

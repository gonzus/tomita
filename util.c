#include <ctype.h>
#include <sys/stat.h>
#include "stb_sprintf.h"
#include "log.h"
#include "util.h"

unsigned file_slurp(const char* path, Buffer* b) {
  FILE* fp = 0;
  unsigned len = 0;
  do {
    fp = fopen(path, "r");
    if (!fp) break;

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
  if (fp) {
    fclose(fp);
  }
  return len;
}

unsigned file_spew(const char* path, Slice s) {
  FILE* fp = 0;
  unsigned len = 0;
  do {
    fp = fopen(path, "w");
    if (!fp) break;

    unsigned nw = fwrite(s.ptr, 1, s.len, fp);
    if (nw != s.len) {
      len = 0;
      break;
    }
    len = s.len;
  } while (0);
  if (fp) {
    fclose(fp);
  }
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

static void dump_line(int row, const char* byte, int white, const char* text) {
    fprintf(stderr, "%06x | %s%*s | %-16s |\n", row, byte, white, "", text);
}

void dump_bytes(const void* ptr, size_t len) {
    fprintf(stderr, "bytes at %p, len %lu\n", ptr, len);
    char byte[16*3+1];
    int bpos = 0;
    char text[16+1];
    int dpos = 0;
    int row = 0;
    int col = 0;
    unsigned char* ucp = (unsigned char*) ptr;
    for (size_t j = 0; j < len; ++j) {
        unsigned char uc = ucp[j];
        unsigned int ui = (unsigned int) uc;
        bpos += stbsp_sprintf(byte + bpos, "%s%02x", col ? " " : "", ui);
        dpos += stbsp_sprintf(text + dpos, "%c", uc <= 0x7f && isprint(uc) ? uc : '.');
        col++;
        if (col == 16) {
            dump_line(row, byte, 0, text);
            col = bpos = dpos = 0;
            row += 16;
        }
    }
    if (dpos > 0) {
        dump_line(row, byte, (16-col)*3, text);
    }
}

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "stb_sprintf.h"
#include <ctype.h>
#include <string.h>

#define BUFFER_DEFAULT_CAPACITY BUFFER_DESIRED_SIZE // default size for Buffer
#define BUFFER_GROWTH_FACTOR 2 // how Buffer grows when needed

static void buffer_adjust(Buffer *b, uint32_t cap);
static void buffer_append_ptr_len(Buffer *b, const char *ptr, uint32_t len);

void buffer_build(Buffer *b) {
  memset(b, 0, sizeof(Buffer));
  b->ptr = b->buf;
  b->cap = BUFFER_DATA_SIZE;
}

void buffer_destroy(Buffer *b) {
  if (BUFFER_FLAG_CHK(b, BUFFER_FLAG_PTR_IN_HEAP)) {
    BUFFER_FLAG_CLR(b, BUFFER_FLAG_PTR_IN_HEAP);
    b->ptr = memory_realloc(b->ptr, 0);
  }
}

void buffer_ensure_total(Buffer *b, uint32_t total) {
  uint32_t changes = 0;
  uint32_t current = b->cap;
  while (total > current) {
    ++changes;
    uint32_t next = current <= BUFFER_DATA_SIZE
                        ? BUFFER_DEFAULT_CAPACITY
                        : current * BUFFER_GROWTH_FACTOR;
    current = next;
  }
  if (changes) {
    if (BUFFER_FLAG_CHK(b, BUFFER_FLAG_PTR_IN_HEAP)) {
      buffer_adjust(b, current);
    } else {
      // Buffer is using stack-allocated buf and need more, switch to ptr
      uint32_t old = b->cap;
      b->ptr = 0;
      b->cap = 0;
      buffer_adjust(b, current);
      memcpy(b->ptr, b->buf, old);
      BUFFER_FLAG_SET(b, BUFFER_FLAG_PTR_IN_HEAP);
    }
  }
}

Slice buffer_slice(const Buffer *b) {
  Slice s = slice_from_memory(b->ptr, b->len);
  return s;
}

void buffer_pack(Buffer *b) {
  if (!BUFFER_FLAG_CHK(b, BUFFER_FLAG_PTR_IN_HEAP)) {
    // Buffer is using stack-allocated buf, don't touch
    return;
  }

  // Buffer is using heap-allocated ptr
  if (b->len < BUFFER_DATA_SIZE) {
    // Enough space in buf, switch to it
    memcpy(b->buf, b->ptr, b->len);
    buffer_adjust(b, 0);
    b->ptr = b->buf;
    BUFFER_FLAG_CLR(b, BUFFER_FLAG_PTR_IN_HEAP);
  } else {
    // Not enough space in buf, remain in ptr
    buffer_adjust(b, b->len);
  }
}

void buffer_set_to_slice(Buffer *b, Slice s, bool zero) {
  buffer_clear(b);
  buffer_append_slice(b, s);
  if (zero) {
    buffer_null_terminate(b);
  }
}

void buffer_append_byte(Buffer *b, char t) {
  buffer_ensure_extra(b, 1);
  b->ptr[b->len++] = t;
}

void buffer_append_string(Buffer *b, const char *str, int len) {
  if (len <= 0) {
    len = str ? strlen(str) : 0;
  }
  buffer_append_ptr_len(b, str, len);
}

void buffer_append_slice(Buffer *b, Slice s) {
  buffer_append_ptr_len(b, s.ptr, s.len);
}

void buffer_append_buffer(Buffer *b, const Buffer *buf) {
  buffer_append_ptr_len(b, buf->ptr, buf->len);
}

void buffer_append_hex(Buffer *b, const char *hex) {
  unsigned char c = 0;
  unsigned l = 0;
  for (unsigned p = 0; hex[p] != '\0'; ++p) {
    unsigned char h = hex[p];
    if (isspace(h))
      continue;
    unsigned ok = 0;
    c <<= 4;
    if (h >= '0' && h <= '9') {
      c |= h - '0' + 0;
      ok = 1;
    }
    if (h >= 'a' && h <= 'f') {
      c |= h - 'a' + 10;
      ok = 1;
    }
    if (h >= 'A' && h <= 'F') {
      c |= h - 'A' + 10;
      ok = 1;
    }
    ++l;
    if (!ok) {
      LOG_FATAL("Invalid hex character at pos %u: %x %c", p, h, h);
    }
    if (l == 2) {
      buffer_append_byte(b, c);
      l = 0;
      c = 0;
    }
  }
}

void buffer_format_signed(Buffer *b, long long l) {
  char cstr[99];
  uint32_t clen = stbsp_sprintf(cstr, "%lld", l);
  buffer_append_ptr_len(b, cstr, clen);
}

void buffer_format_unsigned(Buffer *b, unsigned long long l) {
  char cstr[99];
  uint32_t clen = stbsp_sprintf(cstr, "%llu", l);
  buffer_append_ptr_len(b, cstr, clen);
}

void buffer_format_double(Buffer *b, double d) {
  char cstr[99];
  uint32_t clen = stbsp_sprintf(cstr, "%f", d);
  buffer_append_ptr_len(b, cstr, clen);
}

static char *vprint_cb(const char *buf, void *user, int len) {
  Buffer *b = (Buffer *)user;
  buffer_append_ptr_len(b, buf, len);
  return (char *)buf;
}

void buffer_format_vprint(Buffer *b, const char *fmt, va_list ap) {
  char buf[STB_SPRINTF_MIN];
  stbsp_vsprintfcb(vprint_cb, b, buf, fmt, ap);
}

void buffer_format_print(Buffer *b, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  buffer_format_vprint(b, fmt, ap);
  va_end(ap);
}

static void buffer_adjust(Buffer *b, uint32_t cap) {
  char *ptr = memory_realloc(b->ptr, cap);
  b->ptr = ptr;
  b->cap = cap;
}

static void buffer_append_ptr_len(Buffer *b, const char *ptr, uint32_t len) {
  if (len <= 0) {
    return;
  }
  buffer_ensure_extra(b, len);
  memcpy(b->ptr + b->len, ptr, len);
  b->len += len;
}

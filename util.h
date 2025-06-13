#pragma once

#include "buffer.h"

// quiet down the compiler about an unused variable
#define UNUSED(x) (void) x

// compute the length on an array
#define ALEN(a) (unsigned) (sizeof(a) / sizeof(a[0]))

// Read the contents of a file given by path, into a Buffer.
// Return the number of bytes read.
unsigned file_slurp(const char* path, Buffer* b);

// Write the contents of a slice into a file given by path.
// Return the number of bytes written.
unsigned file_spew(const char* path, Slice s);

// Parse a slice, starting at pos, skipping white space.
// Return the updated pos.
unsigned skip_spaces(Slice line, unsigned pos);

// Parse a slice, starting at pos, for a natural number.
// Return the updated pos.
unsigned next_number(Slice line, unsigned pos, unsigned* number);

// Parse a slice, starting at pos, for a string with format [XXXXX] (brackets included).
// Return the updated pos.
unsigned next_string(Slice line, unsigned pos, Slice* string);

// Dump a block of bytes into stderr with nice formatting.
void dump_bytes(const void* ptr, unsigned len);

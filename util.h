#pragma once

#include "buffer.h"

// quiet down the compiler about an unused variable
#define UNUSED(x) (void) x

// Read the contents of a file given by path, into a Buffer.
// Return the number of bytes read.
unsigned slurp_file(const char* path, Buffer* b);

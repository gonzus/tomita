#pragma once

// quiet down the compiler about an unused variable
#define UNUSED(x) (void) x

// Read the contents of a file given by path, into a (preallocated) character
// buffer with a given capacity.
//
// Return the number of bytes read.
unsigned slurp_file(const char* path, char* buf, unsigned cap);

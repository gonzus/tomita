#ifndef MEMORY_H_
#define MEMORY_H_

/*
 * Functions to deal with memory.
 */

// Realloc a chunk of memory to a given size.  Useful to:
// len == 0            free memory pointed to by ptr
// len > 0, ptr == 0   allocate a new chunk of memory of len bytes
// len > 0, ptr != 0   reallocate memory of len bytes (larger or smaller)
void* memory_realloc(void* ptr, unsigned len);

#endif

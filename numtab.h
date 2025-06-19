#pragma once

// TODO: make hash table growable?
enum {
  NUMTAB_HASH_MAX = 0x100,
};

typedef struct Number {
  unsigned val;
  void* ptr;
  struct Number* next;
} Number;

// a number (hash) table -- for 32 bit integers
typedef struct NumTab {
  Number* table[NUMTAB_HASH_MAX]; // the actual table
} NumTab;

// Create an empty number table.
NumTab* numtab_create(void);

// Destroy a number table created with numtab_create().
void numtab_destroy(NumTab* numtab);

// Clear all contents of a number table -- leave it as just created.
void numtab_clear(NumTab* numtab);

// Print a number table in a human-readable format.
void numtab_show(NumTab* numtab);

// Look up a number by value, and create the entry if not found and we were told.
// Return the found / created number, or null if not found and not created.
Number* numtab_lookup(NumTab* numtab, unsigned val, unsigned char insert);

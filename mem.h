#pragma once

void *Allocate(unsigned Bytes);

void *Reallocate(void *X, unsigned Bytes);

char *CopyB(const char *ptr, unsigned len);
char *CopyS(const char *S);

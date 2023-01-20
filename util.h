#pragma once

#define UNUSED(x) (void) x

unsigned slurp_file(const char* path, char* buf, unsigned cap);

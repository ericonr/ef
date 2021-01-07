#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>

#include "string-array.h"

void read_entries_from_stream(struct str_array *, int, FILE *);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);

#endif

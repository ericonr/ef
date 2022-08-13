#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdio.h>

#include <curses.h>

#include "string-array.h"

void read_entries_from_stream(struct str_array *, int, FILE *);
void print_prompt(WINDOW *, struct str_array *, bool);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);

#endif

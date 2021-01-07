#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "string-array.h"

void add_entry(struct str_array *a, const char *s)
{
	if (a->n == a->c) {
		if (a->c == 0) a->c = 32;
		else a->c *= 2;
		a->v = realloc(a->v, sizeof(*a->v) * a->c);
		a->m = realloc(a->m, sizeof(*a->m) * a->c);
		if (!a->v || !a->m) {
			perror("realloc");
			exit(1);
		}
	}

	a->v[a->n] = s;
	a->n++;
}

void filter_entries(struct str_array *a, const char *s)
{
	if (!s) {
		a->ms = a->n;
		return;
	}

	a->ms = 0;
	for (size_t i = 0; i < a->n; i++) {
		if ((a->m[i] = strstr(a->v[i], s))) a->ms++;
	}
}

void print_entries(const struct str_array *a)
{
	for (size_t i = 0; i < a->n; i++) {
		printf("entry %zu: %s\n", i, get_entry(a, i));
	}
}

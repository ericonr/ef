#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "string-array.h"
#include "util.h"

void add_entry(struct str_array *a, char *s)
{
	if (a->n == a->c) {
		if (a->c == 0) a->c = 32;
		else a->c *= 2;
		a->v = xrealloc(a->v, sizeof(*a->v) * a->c);
		a->m = xrealloc(a->m, sizeof(*a->m) * a->c);
	}

	a->v[a->n] = s;
	a->n++;
}

char *pop_entry(struct str_array *a)
{
	a->n--;
	return get_entry(a, a->n);
}

typedef char * (*substring_search)(const char *, const char*);

void filter_entries(struct str_array *a, const struct str_array *m)
{
	if (!m) {
		a->ms = a->n;
		for (size_t i = 0; i < a->n; i++) {
			a->m[i] = true;
		}
		return;
	}

	/* find if any strings are capitalized */
	bool capitalized = false;
	for (size_t i = 0; i < m->n; i++) {
		char *s = get_entry(m, i);
		size_t l = strlen(s);
		for (size_t j = 0; j < l; j++) {
			if (isupper(s[j])) {
				capitalized = true;
				break;
			}
		}
		if (capitalized) break;
	}
	/* we only care about case if there is an upper char */
	substring_search fn = capitalized ? strstr : strcasestr;

	a->ms = 0;
	for (size_t i = 0; i < m->n; i++) {
		for (size_t j = 0; j < a->n; j++) {
			/* first token or was previously true */
			if (i == 0 || a->m[j]) {
				/* only count matches in the last round */
				if ((a->m[j] = fn(a->v[j], m->v[i])) && (i == m->n - 1)) {
					a->ms++;
				}
			}
		}
	}
}

#define VOIDTOCHAR(x) (*(const char **)x)
static int str_a_item_cmp(const void *a, const void *b)
{
	return strcmp(VOIDTOCHAR(a), VOIDTOCHAR(b));
}

void sort_entries(struct str_array *a)
{
	qsort(a->v, a->n, sizeof(*a->v), str_a_item_cmp);
}

void print_entries(const struct str_array *a)
{
	for (size_t i = 0; i < a->n; i++) {
		printf("entry %zu: %s\n", i, get_entry(a, i));
	}
}

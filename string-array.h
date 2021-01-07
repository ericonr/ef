#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#include <stdbool.h>

struct str_array {
	const char **v;
	bool *m;
	size_t n, c;
};


static inline const char *get_entry(const struct str_array *a, size_t i)
{
	return a->v[i];
}

static inline const char *get_entry_match(const struct str_array *a, size_t i)
{
	return a->m[i] ? a->v[i] : NULL;
}

void add_entry(struct str_array *, const char *);
void filter_entries(struct str_array *, const char *);
void print_entries(const struct str_array *);

#endif

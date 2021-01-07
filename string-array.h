#ifndef STRING_ARRAY_H
#define STRING_ARRAY_H

#include <stdbool.h>

struct str_array {
	/* strings in the array */
	char **v;
	/* flag to say if they fit a certain condition or not */
	bool *m;
	/* total number of true in m */
	size_t ms;
	/* number of elements, maximum number of elements */
	size_t n, c;
};


static inline char *get_entry(const struct str_array *a, size_t i)
{
	return a->v[i];
}

static inline bool get_entry_match(const struct str_array *a, size_t i)
{
	return a->m[i];
}

static inline char *get_entry_if_match(const struct str_array *a, size_t i)
{
	return get_entry_match(a, i) ? a->v[i] : NULL;
}

void add_entry(struct str_array *, char *);
char *pop_entry(struct str_array *);
void filter_entries(struct str_array *, const struct str_array *);
void print_entries(const struct str_array *);

#endif

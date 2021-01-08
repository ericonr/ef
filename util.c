#include <stdlib.h>
#include <unistd.h>

#include "util.h"

void read_entries_from_stream(struct str_array *a, int delim, FILE *input)
{
	char *line = NULL;
	size_t tmp = 0;
	ssize_t n;
	while ((n = getdelim(&line, &tmp, delim, input)) >= 0) {
		if (n == 1) {
			/* don't match empty line */
			free(line);
			continue;
		}
		if (line[n-1] == '\n') line[n-1] = 0;
		add_entry(a, line);

		line = NULL;
		tmp = 0;
	}
}

void *xmalloc(size_t s) {
	void *r = malloc(s);
	if (r) return r;
	perror("malloc");
	exit(1);
}

void *xrealloc(void *p, size_t s) {
	void *r = realloc(p, s);
	if (r) return r;
	perror("realloc");
	exit(1);
}

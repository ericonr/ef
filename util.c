#include <stdlib.h>
#include <unistd.h>

#include "util.h"

void read_entries_from_stream(struct str_array *a, int delim, FILE *input)
{
	char *line = NULL;
	size_t tmp = 0;
	ssize_t n;
	while ((n = getdelim(&line, &tmp, delim, input)) >= 0) {
		if (n == 0 || n == 1 && line[0] == delim) {
			/* don't match empty line */
			free(line);
			line = NULL;
			continue;
		}
		if (line[n-1] == delim) line[n-1] = '\0';
		add_entry(a, line);

		line = NULL;
		tmp = 0;
	}
}

void *xmalloc(size_t s) {
	void *r = malloc(s);
	if (r) return r;
	perror("malloc");
	quick_exit(1);
}

void *xrealloc(void *p, size_t s) {
	void *r = realloc(p, s);
	if (r) return r;
	perror("realloc");
	quick_exit(1);
}

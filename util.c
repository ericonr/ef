#include "util.h"

void read_entries_from_stream(struct str_array *a, int delim, FILE *input)
{
	char *line = NULL;
	size_t tmp = 0;
	ssize_t n;
	while ((n = getdelim(&line, &tmp, delim, input)) >= 0) {
		if (line[n-1] == '\n') line[n-1] = 0;
		add_entry(a, line);

		line = NULL;
		tmp = 0;
	}
}

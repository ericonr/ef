#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include <curses.h>

enum my_keycodes {
	MY_CTRLW = 7000,
};

static void endwin_void(void)
{
	endwin();
}

static void finish(int sig)
{
	(void)sig;
	quick_exit(1);
}

struct str_array {
	const char **v;
	bool *m;
	size_t n, c;
};

static void add_entry(struct str_array *a, const char *s)
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
	a->m[a->n] = false;
	a->n++;
}

static inline const char *get_entry(const struct str_array *a, size_t i)
{
	return a->v[i];
}

static inline const char *get_entry_match(const struct str_array *a, size_t i)
{
	return a->m[i] ? a->v[i] : NULL;
}

static void filter_entries(struct str_array *a, const char *s)
{
	for (size_t i = 0; i < a->n; i++) {
		a->m[i] = strstr(a->v[i], s);
	}
}

static void print_entries(const struct str_array *a)
{
	for (size_t i = 0; i < a->n; i++) {
		printf("entry %zu: %s\n", i, get_entry(a, i));
	}
}

int main()
{
	signal(SIGINT, finish);
	atexit(endwin_void);
	at_quick_exit(endwin_void);

	char delim = '\n';

	struct str_array entries = { 0 };

	char *line = NULL;
	size_t tmp = 0;
	{
		ssize_t n;
		while ((n = getdelim(&line, &tmp, delim, stdin)) >= 0) {
			if (line[n-1] == '\n') line[n-1] = 0;
			add_entry(&entries, line);

			line = NULL;
			tmp = 0;
		}
		if (dup2(STDERR_FILENO, STDIN_FILENO) != STDIN_FILENO) {
			perror("dup2");
			exit(1);
		}
	}

	initscr();
	nonl();
	cbreak();
	noecho();

	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		// red,green,yellow,blue,cyan,magenta,white
	}
	attrset(COLOR_PAIR(1));

	WINDOW *list = newwin(LINES - 1, 0, 0, 0);
	WINDOW *prompt = newwin(1, 0, LINES - 1, 0);
	if (!list || !prompt) {
		perror("newwin");
		exit(1);
	}
	keypad(prompt, TRUE);
	keypad(list, TRUE);

	for (size_t i = 0; i < entries.n; i++) {
		mvwaddstr(list, i, 0, get_entry(&entries, i));
	}
	wrefresh(list);

	mvwaddstr(prompt, 0, 0, "> ");
	wrefresh(prompt);

	char name[1024] = { 0 };
	size_t n = 0;
	while (n < sizeof name) {
		int c = wgetch(prompt);
		switch (c) {
			case KEY_BACKSPACE:
			case 127:
				name[n] = 0;
				if (n) n--;
				break;
			case KEY_DL:
				name[0] = 0;
				n = 0;
				break;
			default:
				name[n] = c;
				n++;
				break;
		}
		werase(prompt);
		mvwaddstr(prompt, 0, 0, "> ");
		waddstr(prompt, name);
		wrefresh(prompt);
		filter_entries(&entries, name);

		werase(list);
		int line = 0;
		for (size_t i = 0; i < entries.n; i++) {
			const char *e = get_entry_match(&entries, i);
			if (e) mvwaddstr(list, line++, 0, e);
		}
		wrefresh(list);
	}

	return 0;
}

#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>

#include <curses.h>

#include "string-array.h"
#include "util.h"

static void endwin_void(void)
{
	endwin();
}

static const char *final_name;
static FILE* stdout_save;
static void print_name(void)
{
	if (stdout_save && final_name) fputs(final_name, stdout_save);
}

static void finish(int sig)
{
	(void)sig;
	quick_exit(1);
}

int main()
{
	const char delim = '\n';

	struct str_array entries = { 0 };
	read_entries_from_stream(&entries, delim, stdin);
	filter_entries(&entries, NULL);

	int tmp_fd;
	/* use stderr for input instead of stdin, since we get the entries from stdin */
	if (dup2(STDERR_FILENO, STDIN_FILENO) != STDIN_FILENO ||
	    /* use stderr for output as well, since we should only print the result to stdout */
	    (tmp_fd = dup(STDOUT_FILENO)) < 0 ||
	    (stdout_save = fdopen(tmp_fd, "w")) == NULL ||
	    dup2(STDERR_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
		perror("fd dance");
		exit(1);
	}

	atexit(print_name);

	signal(SIGINT, finish);
	atexit(endwin_void);
	at_quick_exit(endwin_void);

	/* curses initialization */
	initscr();
	/* terminal configuration */
	nonl();
	cbreak();
	noecho();

	if (has_colors()) {
		start_color();
		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		// red,green,yellow,blue,cyan,magenta,white
	}
	/* noop function - should use the windows from below */
	attrset(COLOR_PAIR(1));

	/* store number of rows that can be displayed in list */
	const int nrows = LINES - 1;

	WINDOW *list = newpad(entries.n, COLS);
	WINDOW *prompt = newwin(1, 0, LINES - 1, 0);
	if (!list || !prompt) {
		perror("newwin");
		exit(1);
	}
	keypad(prompt, TRUE);

	/* inital dump of list */
	for (size_t i = 0; i < entries.n; i++) {
		mvwaddstr(list, i, 0, get_entry(&entries, i));
	}
	prefresh(list, 0, 0, 0, 0, nrows - 1, COLS);

	mvwaddstr(prompt, 0, 0, "> ");
	wrefresh(prompt);

	struct str_array toks = { 0 };
	size_t n, cap;
	char *name = NULL;
	/* listx isn't changed anywhere */
	int listx = 0, listy = 0;
	for (;;) {
		if (!name) {
			cap = 1024;
			name = xmalloc(cap);

			n = 0;
			name[n] = 0;
			add_entry(&toks, name);
		}

		bool name_changed = false;
		int c = wgetch(prompt);
		switch (c) {
			/* Ctrl+[ or ESC */
			case 27:
				exit(1);
				break;

			case KEY_ENTER:
			case '\r':
				/* since we are using nonl above, only capture '\r' itself
				 * TODO: actually store the entry name */
				final_name = name;
				exit(0);

			case KEY_DOWN:
			/* Ctrl-N */
			case 14:
				if ((size_t)listy < entries.n && (size_t)nrows < entries.ms ) listy++;
				break;

			case KEY_UP:
			/* Ctrl-P */
			case 16:
				if (listy) listy--;
				break;

			/* Ctrl+W */
			case 23:
				n = 0;
				name[n] = 0;
				name_changed = true;
				break;

			case KEY_BACKSPACE:
			/* DEL */
			case 127:
				/* go back to previous word */
				if (toks.n > 1 && n == 0) {
					free(pop_entry(&toks));
					name = get_entry(&toks, toks.n - 1);
					cap = malloc_usable_size(name);
					n = strlen(name);
					name_changed = true;
					break;
				}
				if (n) n--;
				name[n] = 0;
				name_changed = true;
				break;

			case ' ':
				if (*name) {
					name = NULL;
				}
				name_changed = true;
				break;

			default:
				if (n == cap) {
					cap *= 2;
					name = xrealloc(name, cap);
				}
				name[n] = c;
				n++;
				/* clear chars from previous entries and/or dirty memory */
				name[n] = 0;
				name_changed = true;
				break;
		}
		if (!name_changed) {
			prefresh(list, listy, listx, 0, 0, nrows-1, COLS);
			continue;
		}

		werase(prompt);
		mvwaddstr(prompt, 0, 0, ">");
		for (size_t i = 0; i < toks.n; i++) {
			const char *e = get_entry(&toks, i);
			waddch(prompt, ' ');
			waddstr(prompt, e);
		}
		/* show space if string is empty */
		if (!name) waddch(prompt, ' ');

		wrefresh(prompt);
		if (!name) continue;

		filter_entries(&entries, &toks);

		werase(list);
		int line = 0;
		for (size_t i = 0; i < entries.n; i++) {
			const char *e = get_entry_if_match(&entries, i);
			if (e) mvwaddstr(list, line++, 0, e);
		}
		/* reset vertical position when name changes */
		listy = 0;
		prefresh(list, listy, listx, 0, 0, nrows-1, COLS);
	}

	return 0;
}

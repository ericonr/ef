#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>

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
	setlocale(LC_ALL, "");

	const char delim = '\n';

	struct str_array entries = { 0 };
	read_entries_from_stream(&entries, delim, stdin);
	sort_entries(&entries);
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
	int cattr = COLOR_PAIR(1) | A_BOLD;
	int nattr = A_NORMAL;

	/* list should work as follows:
	 *   - can fit up to entries.n
	 *   - element 0 is at the bottom (entries.n - 1)
	 *   - last element is at the top (0)
	 */

	/* store number of rows that can be displayed in list */
	const int nrows = LINES - 1;
	/* list position in x and y;
	 * listx currently isn't changed anywhere */
	int listy;
	const int listx = 0;
	/* current pick */
	size_t pick = 0;
	/* list size */
	int listsize;

	if (entries.n <= (size_t)nrows) {
		/* list should be at least as big as nrows */
		listsize = nrows;

		listy = 0;
	} else {
		listsize = entries.n;

		/* move to position where we show the end of the list */
		listy = entries.n - nrows;
	}

	WINDOW *list = newpad(listsize, COLS);
	WINDOW *prompt = newwin(1, 0, LINES - 1, 0);
	if (!list || !prompt) {
		perror("newwin");
		exit(1);
	}
	keypad(prompt, TRUE);

	/* initial prompt */
	mvwaddstr(prompt, 0, 0, "> ");
	wrefresh(prompt);

#define WRITELIST(idx) mvwaddstr(list, listsize - idx - 1, 0, get_entry(&entries, idx))
	/* inital dump of list */
	for (size_t i = 0; i < entries.n; i++) {
		if (i == pick) wattrset(list, cattr);
		WRITELIST(i);
		if (i == pick) wattrset(list, nattr);
	}
	prefresh(list, listy, listx, 0, 0, nrows-1, COLS);

	/* variables to control current search token */
	size_t n, cap;
	char *name = NULL;
	/* search tokens */
	struct str_array toks = { 0 };
	for (;;) {
		if (!name) {
			cap = 1024;
			name = xmalloc(cap);

			n = 0;
			name[n] = 0;
			add_entry(&toks, name);
		}

		int pos_change = 0;
		int c = wgetch(prompt);
		switch (c) {
			case 27: /* Ctrl+[ or ESC */
				exit(1);
				break;

			case KEY_ENTER:
			case '\r':
				/* since we are using nonl above, only capture '\r' itself */
				final_name = get_entry(&entries, pick);
				exit(0);

			case KEY_DOWN:
			case 14: /* Ctrl-N */
				pos_change = -1;
				break;

			case KEY_UP:
			case 16: /* Ctrl-P */
				pos_change = +1;
				break;

			/* TODO: treat n==0 */
			case 23: /* Ctrl+W */
				n = 0;
				name[n] = 0;
				break;

			case KEY_BACKSPACE:
			case 127: /* DEL */
				/* go back to previous word */
				if (toks.n > 1 && n == 0) {
					free(pop_entry(&toks));
					name = get_entry(&toks, toks.n - 1);
					cap = malloc_usable_size(name);
					n = strlen(name);
					break;
				}
				if (n) n--;
				name[n] = 0;
				break;

			case ' ':
				if (*name) {
					name = NULL;
				}
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
				break;
		}

		if (pos_change) {
			const size_t old_pick = pick;
			/* XXX: trusting integer overflow to work things out here */
			for (size_t i = pick + pos_change; i < entries.n; i += pos_change) {
				if (get_entry_match(&entries, i)) {
					pick = i;
					break;
				}
			}
			if (pick == old_pick) continue;

			/* clean up appearance */
			wattrset(list, nattr);
			WRITELIST(old_pick);
			wattrset(list, cattr);
			WRITELIST(pick);
			wattrset(list, nattr);

			/* find index inside set of matches */
			size_t index_in_matched = 0;
			for (size_t i = 0; i < pick; i++) {
				if (get_entry_match(&entries, i)) index_in_matched++;
			}

			/* check if all matched entries fit in visible list */
			if (entries.ms <= (size_t)nrows) {
				/* pass */
			} else {
				/* check if pick is above or below */
				const size_t bottom = (entries.ms) - (listy+nrows);
				const size_t top = bottom + nrows - 1;
				if (index_in_matched < bottom) listy++;
				else if (index_in_matched > top) listy--;
			}
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

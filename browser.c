#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

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

	WINDOW *list = newwin(LINES - 1, 0, 0, 0);
	WINDOW *prompt = newwin(1, 0, LINES - 1, 0);
	if (!list || !prompt) {
		perror("newwin");
		exit(1);
	}
	keypad(prompt, TRUE);
	keypad(list, TRUE);

	/* inital dump of list
	 * TODO: needs scroll support */
	for (size_t i = 0; i < entries.n; i++) {
		mvwaddstr(list, i, 0, get_entry(&entries, i));
	}
	wrefresh(list);

	mvwaddstr(prompt, 0, 0, "> ");
	wrefresh(prompt);

	/* limited name size
	 * TODO: use dynamic string and str_array so we can search on a group of tokens */
	char name[1024] = { 0 };
	size_t n = 0;
	while (n < sizeof name) {
		int c = wgetch(prompt);
		switch (c) {
			case KEY_ENTER:
			case '\r':
				/* since we are using nonl above, only capture '\r' itself
				 * TODO: actually store the entry name */
				final_name = name;
				exit(0);
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

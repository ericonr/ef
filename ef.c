#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <malloc.h>
#include <poll.h>
#include <errno.h>
#include <locale.h>

#include <curses.h>

#include "string-array.h"
#include "util.h"

#define MAX_TOK_LEN 128

static void endwin_void(void)
{
	endwin();
	fflush(stdout);
}

static const char *final_name;
static FILE* stdout_save;
static void print_name(void)
{
	if (stdout_save && final_name) fputs(final_name, stdout_save);
}

static int signal_pipe[2];
static void signal_handler(int signum)
{
	write(signal_pipe[1], &signum, sizeof signum);
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");

	char delim = '\n';
	char *query = NULL;

	int opt;
	while ((opt = getopt(argc, argv, "01q:")) != -1) {
		switch (opt) {
			case '0':
				/* set delimiter to NUL char instead of newline */
				delim = '\0';
				break;
			case '1':
				/* ignored for compatibility with fzf */
				break;
			case 'q':
				/* initial query */
				query = optarg;
				break;
			default:
				exit(1);
		}
	}

	if (pipe(signal_pipe)) {
		perror("pipe");
		exit(1);
	}
	if (signal(SIGINT, signal_handler) || signal(SIGTERM, signal_handler)) {
		perror("signal");
		exit(1);
	}

	struct str_array entries = { 0 };
	/* create our own stream for standard input,
	 * so stdin is fresh when we use it with our new file descriptor below */
	int stdin_dup;
	FILE *original_stdin;
	if ((stdin_dup = dup(STDIN_FILENO)) < 0 ||
	    (original_stdin = fdopen(stdin_dup, "r")) == NULL) {
		perror("stdin handling");
		exit(1);
	}
	read_entries_from_stream(&entries, delim, original_stdin);
	fclose(original_stdin);

	/* fast exit cases */
	if (entries.n == 0) exit(0);
	if (entries.n == 1) {
		puts(get_entry(&entries, 0));
		exit(0);
	}
	sort_entries(&entries);
	filter_entries(&entries, NULL);

	/* use the controlling terminal as new stdin/stdout,
	 * since the process ones are used for input and output */
	int tty_fd, stdout_save_fd;
	if ((tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC)) < 0 ||
	    (stdout_save_fd = dup(STDOUT_FILENO)) < 0 ||
	    (stdout_save = fdopen(stdout_save_fd, "we")) == NULL ||
	    dup2(tty_fd, STDIN_FILENO) != STDIN_FILENO ||
	    dup2(tty_fd, STDOUT_FILENO) != STDOUT_FILENO) {
		perror("fd dance");
		exit(1);
	}
	close(tty_fd);

	enum { SIGNAL, STDIN, POLLFD_AMOUNT };
	struct pollfd polls[POLLFD_AMOUNT] = {
		[SIGNAL] = {signal_pipe[0], POLLIN, 0},
		[STDIN] = {STDIN_FILENO, POLLIN, 0},
	};

	atexit(print_name);

	/* curses initialization */
	initscr();
	at_quick_exit(endwin_void);
	atexit(endwin_void);
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
	const size_t nrows = LINES - 1;
	/* list position in x and y;
	 * listx currently isn't changed anywhere */
	int listy;
	const int listx = 0;
	/* current pick */
	size_t pick = 0;
	/* list size */
	int listsize;

	if (entries.n <= nrows) {
		/* list should be at least as big as nrows */
		listsize = nrows;

		listy = 0;
	} else {
		listsize = entries.n;

		/* move to position where we show the end of the list */
		listy = listsize - nrows;
	}

	WINDOW *list = newpad(listsize, COLS);
	WINDOW *prompt = newwin(1, 0, LINES - 1, 0);
	if (!list || !prompt) {
		perror("newwin");
		quick_exit(1);
	}
	keypad(prompt, TRUE);

	/* index in entries, index in matched;
	 * this function could include a wclrtoeol call, but that only hides indexing issues */
	#define WRITELIST(idx, idxm) \
		do{mvwaddstr(list, listsize - idxm - 1, 0, get_entry(&entries, idx));}while(0)

	/* inital dump of list */
	for (size_t i = 0; i < entries.n; i++) {
		if (i == pick) wattrset(list, cattr);
		WRITELIST(i,i);
		if (i == pick) wattrset(list, nattr);
	}
	prefresh(list, listy, listx, 0, 0, nrows-1, COLS);

	/* variables to control current search token */
	const size_t cap = MAX_TOK_LEN; /* words bigger than this aren't allowed */
	size_t n;
	char *name = NULL;
	/* search tokens */
	struct str_array toks = { 0 };

	if (query) {
		/* the intial query can be edited,
		 * so it needs to be the same sort of storage */
		name = xmalloc(cap);
		strncpy(name, query, cap);
		add_entry(&toks, name);

		name = NULL;
	}
	print_prompt(prompt, &toks, true);

	/* index inside set of matches */
	size_t index_in_matched = 0;
	for (;;) {
		if (poll(polls, POLLFD_AMOUNT, -1) < 0) {
			if (errno == EINTR) continue;

			perror("poll");
			quick_exit(1);
		}

		if (polls[SIGNAL].revents & POLLIN) {
			int signum;
			read(polls[SIGNAL].fd, &signum, sizeof signum);
			if (signum == SIGINT || signum == SIGTERM) {
				quick_exit(1);
			}
		}

		if (!(polls[STDIN].revents & POLLIN)) continue;

		if (!name) {
			name = xmalloc(cap);

			n = 0;
			name[n] = 0;
			add_entry(&toks, name);
		}

		int pos_change = 0;
		int c = wgetch(prompt);
		switch (c) {
			case 27: /* Ctrl+[ or ESC */
				quick_exit(1);
				break;

			case KEY_ENTER:
			case '\r': /* since we are using nonl above, only capture '\r' itself */
				/* only set final_name if there are actual results */
				if (entries.ms) final_name = get_entry(&entries, pick);
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
				/* XXX: not unicode aware;
				 * this is reasonable for now, since otherwise we'd have to
				 * implement character deletion with unicode */
				if (n < cap && isprint((unsigned char)c)) {
					name[n] = c;
					n++;
					/* clear chars from previous entries and/or dirty memory */
					name[n] = 0;
				}
				break;
		}

		if (pos_change) {
			/* bail out if there aren't any matches at all */
			if (entries.ms == 0) continue;

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
			WRITELIST(old_pick, index_in_matched);
			wattrset(list, cattr);

			/* we know there's another item in that direction */
			index_in_matched += pos_change;

			WRITELIST(pick, index_in_matched);
			wattrset(list, nattr);

			/* check if all matched entries fit in visible list */
			if (entries.ms <= nrows) {
				/* pass */
			} else {
				/* check if pick is above or below visible indexes */
				const size_t bottom = listsize - (listy+nrows);
				const size_t top = bottom + nrows - 1;
				if (index_in_matched < bottom) listy++;
				else if (index_in_matched > top) listy--;
			}
			prefresh(list, listy, listx, 0, 0, nrows-1, COLS);

			continue;
		}

		/* show space if name is NULL */
		print_prompt(prompt, &toks, !name);

		/* name==NULL means the search results won't change,
		 * so we don't need to search again */
		if (!name) continue;

		filter_entries(&entries, &toks);

		/* keep index_in_matched if possible;
		 * this will probably change what we have selected,
		 * but it's what fzf does, for example.
		 * we will find out what pick it corresponds to in the loop below */
		if (index_in_matched >= entries.ms) {
			if (entries.ms == 0) index_in_matched = 0;
			else index_in_matched = entries.ms - 1;
		}

		werase(list);
		if (entries.ms == 0) {
			/* don't loop through array if there aren't results */
			listy = 0;
			prefresh(list, listy, listx, 0, 0, nrows-1, COLS);
			continue;
		}
		size_t line = 0;
		for (size_t i = 0; i < entries.n; i++) {
			if (get_entry_match(&entries, i)) {
				if (line == index_in_matched) {
					pick = i;
					wattrset(list, cattr);
				}
				WRITELIST(i, line);
				if (line == index_in_matched) wattrset(list, nattr);

				line++;
			}
		}

		/* starting position at the bottom of list */
		listy = listsize - nrows;
		if (entries.ms <= nrows) {
			/* pass */
		} else {
			const size_t top = nrows - 1;
			/* fix listy so index_in_matched is in shown in view */
			if (index_in_matched > top) {
				size_t diff = index_in_matched - top;
				/* (index_in_matched <= entries.ms <= listsize)
				 * ([index_in_matched - nrows + 1] <= [listsize - nrows] + 1)
				 * (diff <= listy + 1), which only guarantees (listy - diff >= -1).
				 * therefore, the conditional is necessary */
				if ((size_t)listy > diff) listy -= diff;
				else listy = 0;
			}
		}

		prefresh(list, listy, listx, 0, 0, nrows-1, COLS);

#if 0
		/* version that tries to keep the cursor over the *value* it was on,
		 * instead of the *position* */
		index_in_matched = 0;
		if (get_entry_match(&entries, pick)) {
			/* if pick is still in matched set, we need to find its new index there */
			for (size_t i = 0; i < pick; i++) if (get_entry_match(&entries, i)) index_in_matched++;
		} else if (entries.ms) {
			/* we only try to find a new pick if we know it exists;
			 * we use a lower value if available */
			size_t old_pick = pick, prev_pick = 0;
			for (size_t i = 0; i < entries.n && index_in_matched < entries.ms; i++) {
				if (get_entry_match(&entries, i)) {
					if (i > pick && index_in_matched) {
						/* found match after old pick and there was at least one match before that,
						 * so we select the one right before this new one */
						pick = prev_pick;
						break;
					}
					index_in_matched++;
					prev_pick = i;
				}
			}
			if (pick == old_pick) {
				/* if it didn't set pick before, set now */
				index_in_matched--;
				pick = prev_pick;
			}
		}
		/* print... */
		listy = listsize - nrows;
		if (entries.ms > nrows) {
			const size_t top = nrows - 1;
			if (index_in_matched > top) {
				size_t diff = index_in_matched - top;
				if ((size_t)listy > diff) listy -= diff;
				else listy = 0;
			}
		}
		/* refresh... */
#endif
	}

	return 0;
}

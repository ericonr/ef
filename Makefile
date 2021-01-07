SRC = browser.c
EXE = browser
CFLAGS = -Wall -Wextra
LDLIBS = -lcurses

all: $(EXE)

clean:
	rm -f $(EXE)

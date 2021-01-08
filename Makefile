PREFIX = /usr/local

SRC = ef.c string-array.c util.c
OBJ = $(SRC:%.c=%.o)
EXE = ef
CFLAGS = -Wall -Wextra -g
LDLIBS = -lcurses

.PHONY: all install clean

all: $(EXE)

ef: $(OBJ)
$(OBJ): string-array.h util.h

install: all
	install -Dm755 ef $(DESTDIR)$(PREFIX)/bin/ef

clean:
	rm -f $(EXE) $(OBJ)

SRC = browser.c string-array.c util.c
OBJ = $(SRC:%.c=%.o)
EXE = browser
CFLAGS = -Wall -Wextra
LDLIBS = -lcurses

all: $(EXE)

browser: $(OBJ)
$(OBJ): string-array.h util.h

clean:
	rm -f $(EXE) $(OBJ)

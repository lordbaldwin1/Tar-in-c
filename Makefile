CC = gcc
DEBUG = -g
DEFINES =
CFLAGS = $(DEBUG) -Wall -Wshadow -Wunreachable-code -Wredundant-decls \
	 -Wno-return-local-addr -Wunsafe-loop-optimizations \
	 -Wuninitialized -Werror
PROG1 = viktar
PROGS = $(PROG1)
INCLUDES = viktar.h

all: $(PROGS)

$(PROG1): $(PROG1).o
	$(CC) $(CFLAGS) -o $@ $^

$(PROG1).o: $(PROG1).c $(INCLUDES)
	$(CC) $(CFLAGS) -c $<

clean cls:
	rm -f $(PROGS) *.o *~ \#*

tar:
	tar cvfa viktar_${LOGNAME}.tar.gz *.[ch] [mM]akefile

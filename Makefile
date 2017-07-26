NAME=ysh
CC=gcc
CFLAGS=-O0 -g -Wall -fPIE -Werror -Wextra -Wno-unused -rdynamic -std=gnu11 -I.
LDFLAGS=-fPIE -rdynamic
LIBS=-lm

OBJ  = $(shell ls */*.c | sed 's|\.c|.o|g') $(shell ls *.c | sed 's|\.c|.o|g')

%.o: %.c
	$(CC) -c -o $@ $(CFLAGS) $(CPPFLAGS) $<

all: ysh

ysh: $(OBJ) $(MODOBJ)
	$(CC) -o $(NAME) $(LDFLAGS) $(OBJ) $(MODOBJ) $(MAIN) $(LIBS)

.PHONY: clean
clean:
	rm -f *.o */*.o */*/*.o ysh


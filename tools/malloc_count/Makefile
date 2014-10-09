CC = gcc
CFLAGS = -g -W -Wall -ansi -I..
LDFLAGS =
LIBS = -ldl
OBJS = malloc_count.o stack_count.o

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

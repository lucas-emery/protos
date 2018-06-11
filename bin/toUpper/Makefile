CC=gcc
CFLAGS=-Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

default: all

all:	$(OBJS)
	$(CC) -o toUpper $^ $(CFLAGS)
	rm *.o

.PHONY: clean

clean:
	rm -f *.o
	rm toUpper

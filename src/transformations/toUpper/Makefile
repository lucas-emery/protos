IDIR=../../include
CC=gcc
CFLAGS=-I$(IDIR) -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

default: all

all:	$(OBJS)
	$(CC) -o ../../../bin/toUpper $^ $(CFLAGS) $(CLIBS)
	rm *.o

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~

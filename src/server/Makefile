IDIR=../../include
CC=gcc
CFLAGS=-I$(IDIR) -Wall

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

CLIBS=-lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

default: all

all:	$(OBJS)
	$(CC) -o ../../server $^ $(CFLAGS) $(CLIBS)
	rm *.o

debug:	$(OBJS)
		$(CC) -o ../../server -g $^ $(CFLAGS) $(CLIBS)
		rm *.o

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~

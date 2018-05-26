IDIR=../../include
CC=gcc
CFLAGS=-I$(IDIR)

SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

default: all

all:	$(OBJS)
	$(CC) -o ../../server $^ $(CFLAGS)
	rm *.o

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(IDIR)/*~


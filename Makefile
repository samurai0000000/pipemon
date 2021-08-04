#
# Makefile
#
# Copyright (C) 2021, Charles Chiou

CC ?=		gcc
CFLAGS =	-Wall -O3 -g
LDFLAGS =
RM ?=		rm -f

.PHONY: default clean distclean

TARGETS =	pipemon

OBJS =		pipemon.o

default: $(TARGETS)

pipemon: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *~ *.o $(TARGETS)

distclean: clean

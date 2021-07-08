#
# Makefile
#
# Copyright (C) 2021, Charles Chiou

CC =		gcc
CFLAGS =	-Wall -O3 -g
LDFLAGS =

.PHONY: default clean distclean

TARGETS =	pipemon

default: $(TARGETS)

pipemon: pipemon.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(wildcard *.h)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *~ *.o $(TARGETS)

distclean: clean

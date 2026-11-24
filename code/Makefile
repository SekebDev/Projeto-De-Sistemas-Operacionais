# Makefile - Linux / FreeBSD
#
#   make         -> compila bench e os testes
#   make test    -> roda os testes
#   make clean

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -D_POSIX_C_SOURCE=200809L
LDLIBS  = -pthread

TESTES  = ../testes
LIB     = platform_thread.o bbuffer_cond.o bbuffer_sem.o

all: bench test_platform test_bbuffer

bench: bench.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_platform: $(TESTES)/test_platform.c platform_thread.o
	$(CC) $(CFLAGS) -I. -o $@ $^ $(LDLIBS)

test_bbuffer: $(TESTES)/test_bbuffer.c $(LIB)
	$(CC) $(CFLAGS) -I. -o $@ $^ $(LDLIBS)

%.o: %.c platform_thread.h bbuffer.h
	$(CC) $(CFLAGS) -c $<

test: test_platform test_bbuffer
	./test_platform
	./test_bbuffer

clean:
	rm -f *.o bench test_platform test_bbuffer

.PHONY: all test clean

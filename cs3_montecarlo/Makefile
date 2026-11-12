# Case Study 3 - Monte Carlo option pricing
CC      = gcc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11
LDLIBS  = -lm

all: serial

serial: serial.c mc_common.h
	$(CC) $(CFLAGS) -o $@ serial.c $(LDLIBS)

clean:
	rm -f serial parallel

.PHONY: all clean

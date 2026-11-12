# Case Study 4 - PageRank (OpenMP)
CC      = gcc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11
LDLIBS  = -lm
SCALE  ?= 20

all: gen serial parallel

gen: gen.c pr_common.h
	$(CC) $(CFLAGS) -o $@ gen.c $(LDLIBS)
serial: serial.c pr_common.h
	$(CC) $(CFLAGS) -o $@ serial.c $(LDLIBS)
parallel: parallel.c pr_common.h
	$(CC) $(CFLAGS) -fopenmp -o $@ parallel.c $(LDLIBS)

graph_s$(SCALE).bin: gen
	./gen $(SCALE)

# pull and push must match the serial ranks within the tolerance for every p
check: all graph_s$(SCALE).bin
	@./serial graph_s$(SCALE).bin > /dev/null
	@for v in pull push; do for p in 1 2 4; do for s in static dynamic,256 guided; do \
	  OMP_NUM_THREADS=$$p OMP_SCHEDULE=$$s ./parallel $$v graph_s$(SCALE).bin | \
	  sed -n "s/^verify *: /$$v p=$$p $$s: /p"; done; done; done

clean:
	rm -f gen serial parallel ranks_serial.bin

.PHONY: all check clean

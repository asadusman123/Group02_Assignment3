# Case Study 3 - Monte Carlo option pricing (OpenMP)
CC      = gcc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11
LDLIBS  = -lm

all: serial parallel

serial: serial.c mc_common.h
	$(CC) $(CFLAGS) -o $@ serial.c $(LDLIBS)

parallel: parallel.c mc_common.h
	$(CC) $(CFLAGS) -fopenmp -o $@ parallel.c $(LDLIBS)

# quick correctness check: p=1 must equal serial bit-for-bit,
# and two runs with the same p must agree exactly
check: all
	@./serial 10000000 42 | grep RESULT | sed 's/.*price=\([^ ]*\).*/serial      \1/'
	@for p in 1 2 4; do \
	  a=$$(OMP_NUM_THREADS=$$p ./parallel 10000000 42 | grep RESULT | sed 's/.*price=\([^ ]*\).*/\1/'); \
	  b=$$(OMP_NUM_THREADS=$$p ./parallel 10000000 42 | grep RESULT | sed 's/.*price=\([^ ]*\).*/\1/'); \
	  echo "parallel p=$$p $$a  (rerun $$b)"; \
	done

clean:
	rm -f serial parallel

.PHONY: all check clean

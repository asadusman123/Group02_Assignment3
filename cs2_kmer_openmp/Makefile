# Case Study 2 - parallel k-mer counting (OpenMP)
CC      = gcc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11

all: serial parallel membench

serial: serial.c kmer_common.h
	$(CC) $(CFLAGS) -o $@ serial.c

parallel: parallel.c kmer_common.h
	$(CC) $(CFLAGS) -fopenmp -o $@ parallel.c

# correctness: B and C must match the serial checksum for every p, k and
# distribution; A is expected to FAIL. Uses L = 2e7 to stay quick.
check: all
	@for k in 8 12; do for d in uniform skewed; do \
	  s=$$(./serial $$k 20000000 $$d | sed -n 's/.*checksum=\([^ ]*\).*/\1/p'); \
	  echo "k=$$k $$d serial checksum $$s"; \
	  for v in A B C; do for p in 1 2 4; do \
	    r=$$(OMP_NUM_THREADS=$$p ./parallel $$v $$k 20000000 $$d 2026 verify | grep RESULT); \
	    c=$$(echo "$$r" | sed -n 's/.*checksum=\([^ ]*\).*/\1/p'); \
	    t=$$(echo "$$r" | sed -n 's/.*total=\([^ ]*\).*/\1/p'); \
	    b=$$(echo "$$r" | sed -n 's/.*badbins=\([^ ]*\).*/\1/p'); \
	    [ "$$c" = "$$s" ] && ok=MATCH || ok=DIFF; \
	    echo "   $$v p=$$p total=$$t badbins=$$b $$ok"; \
	  done; done; done; done

membench: membench.c
	$(CC) $(CFLAGS) -fopenmp -o $@ membench.c

clean:
	rm -f serial parallel membench

.PHONY: all check clean

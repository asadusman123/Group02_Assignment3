# Case Study 5 - distributed sample sort (MPI)
CC      = gcc
MPICC   = mpicc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11
LDLIBS  = -lm
MPIRUN4 ?= mpirun --map-by :OVERSUBSCRIBE

all: serial parallel pingpong

serial: serial.c ss_common.h
	$(CC) $(CFLAGS) -Wno-unused-function -o $@ serial.c $(LDLIBS)
parallel: parallel.c ss_common.h
	$(MPICC) $(CFLAGS) -Wno-unused-function -o $@ parallel.c $(LDLIBS)
pingpong: pingpong.c
	$(MPICC) $(CFLAGS) -o $@ pingpong.c

# both distributions, both splitter rules, p = 1, 2, 4 (N = 1e7, radix to keep it quick)
check: all
	@for d in uniform zipf; do for t in tie notie; do for p in 1 2 4; do \
	  if [ $$p -gt 2 ]; then R="$(MPIRUN4)"; else R="mpirun"; fi; \
	  o=$$($$R -np $$p ./parallel 10000000 $$d 1 radix $$t); \
	  v=$$(echo "$$o" | sed -n 's/^verify *: \([A-Z]*\).*/\1/p'); \
	  r=$$(echo "$$o" | sed -n 's/.*max\/mean = \([0-9.]*\).*/\1/p'); \
	  echo "$$d $$t p=$$p: $$v  (max/mean $$r)"; \
	done; done; done

clean:
	rm -f serial parallel pingpong

.PHONY: all check clean

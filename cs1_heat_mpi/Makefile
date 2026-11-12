# Case Study 1 - heat diffusion with MPI
CC      = gcc
MPICC   = mpicc
CFLAGS  = -O3 -march=native -Wall -Wextra -std=c11
LDLIBS  = -lm
# mpirun for p > physical cores (2 on the test laptop): allow oversubscription
MPIRUN4 ?= mpirun --map-by :OVERSUBSCRIBE

all: serial parallel

serial: serial.c heat_common.h
	$(CC) $(CFLAGS) -o $@ serial.c $(LDLIBS)

parallel: parallel.c heat_common.h
	$(MPICC) $(CFLAGS) -o $@ parallel.c $(LDLIBS)

# bit-for-bit check of every decomposition / communication mode against serial
check: all
	@./serial 512 200 10 /tmp/heat_serial.bin > /dev/null
	@for d in 1d 2d; do for m in blocking nonblocking nbtest; do for p in 1 2 4; do \
	  if [ $$p -gt 2 ]; then R="$(MPIRUN4)"; else R="mpirun"; fi; \
	  $$R -np $$p ./parallel 512 200 10 $$d $$m /tmp/heat_par.bin > /dev/null; \
	  if cmp -s /tmp/heat_serial.bin /tmp/heat_par.bin; then r=IDENTICAL; else r=DIFFERENT; fi; \
	  echo "$$d $$m p=$$p: $$r"; \
	done; done; done

clean:
	rm -f serial parallel

.PHONY: all check clean

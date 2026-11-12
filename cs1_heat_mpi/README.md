# Case Study 1 - Weather / Heat Diffusion Simulation (MPI)

## Build
    make            # serial (gcc) and parallel (mpicc), -O3 -march=native -std=c11
    make check      # every decomposition x halo mode x p=1,2,4 must give a final grid
                    # byte-identical to serial (N=512, T=200)

## Run
    ./serial [N] [T] [K] [dumpfile]
    mpirun --bind-to core -np p ./parallel [N] [T] [K] [1d|2d] [blocking|nonblocking|nbtest] [dumpfile]
    mpirun --map-by :OVERSUBSCRIBE -np 4 ./parallel ...      # p > 2 physical cores (Open MPI 5)

Defaults: `N = 1024`, `T = 500`, `K = 50`, `2d`, `nonblocking`. With a dumpfile, the final grid is
written as N*N raw doubles (MPI-IO in the parallel version), so `cmp serial.bin parallel.bin` checks
bit-for-bit equality.

## Problem
N x N grid, explicit 5-point stencil, alpha = 0.2, T time steps, two grids swapped every step.
Fixed boundary: top 100, left 75, right 50, bottom 0. The interior starts at 0.
Every K steps the maximum absolute change is reduced with `MPI_Allreduce(MAX)`.

## Decomposition and halo modes
- `1d`: `MPI_Cart_create` with dims p x 1 (row blocks).
- `2d`: dims from `MPI_Dims_create(p, 2)`, giving 2 x 2 at p = 4. At p = 2 both give 2 x 1.
- `blocking`: four `MPI_Sendrecv` calls (rows contiguous, columns as an `MPI_Type_vector`), then update.
- `nonblocking`: post `MPI_Irecv`/`MPI_Isend`, update the inner cells, `MPI_Waitall`, update the outer frame.
- `nbtest`: like nonblocking, plus `MPI_Testall` every 16 rows so the MPI library can progress the transfers.

## Timing
`MPI_Barrier` + `MPI_Wtime` around the time loop. Reported times are the maximum over ranks, with
per-phase times for compute, halo communication and allreduce.

## Benchmarks
    ./bench.sh && python3 analyze.py      # ~25 min; results/raw.log, tables.md, scaling.png, phases.png
Includes verification, strong scaling (N = 1024 and 4096, T = 500), K = 1 vs K = 50, and weak
scaling (N_p = 2048 * sqrt(p)).

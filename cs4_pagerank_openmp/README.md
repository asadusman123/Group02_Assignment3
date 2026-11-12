# Case Study 4 - Parallel PageRank (OpenMP)

## Build and input
    make                 # gen, serial, parallel   (gcc -O3 -march=native)
    ./gen 20             # R-MAT scale 20: 1,048,576 vertices, ~16.1 M edges -> graph_s20.bin (~131 MiB, not committed)
    make check           # pull and push vs serial ranks for p = 1,2,4 and three schedules

The input is a synthetic power-law graph from an R-MAT generator (Chakrabarti, Zhan, Faloutsos,
SIAM SDM 2004) with the Graph500 parameters a,b,c,d = 0.57, 0.19, 0.19, 0.05, edge factor 16 and
seed 2026. Self-loops and duplicate edges are removed. The graph is stored in CSR in both
directions: out-edges for push, in-edges for pull. Regenerating with the same seed gives the
identical file.

## Run
    ./serial  [graph.bin] [eps] [maxiter] [ranks_out.bin]          # writes ranks_serial.bin
    OMP_NUM_THREADS=p OMP_SCHEDULE=dynamic,256 OMP_PLACES=cores OMP_PROC_BIND=close \
        ./parallel pull|push|pushna [graph.bin] [eps] [maxiter] [ref_ranks.bin]

Defaults: `graph_s20.bin`, `eps = 1e-9` (L1 norm of the change), `maxiter = 200`. d = 0.85.
Dangling vertices (out-degree 0) have their rank spread uniformly over all vertices, so the ranks
always sum to 1.

| VERSION | what it is |
|---|---|
| `pull` | threads own destination vertices and gather from in-neighbours (no write conflicts) |
| `push` | threads own source vertices and scatter to out-neighbours with `omp atomic` |
| `pushna` | push without atomics: a data race, used only at p = 1 to price the atomic itself |

The edge loop uses `schedule(runtime)`, so `OMP_SCHEDULE` selects static / dynamic,c / guided.
Output includes the load-imbalance ratio R (slowest/mean thread time in the edge loop) and the
L1 distance to the serial ranks. The tolerance is 1e-9 (PASS/FAIL).

## Benchmarks
    ./bench.sh && python3 analyze.py     # ~20 min; results/: raw.log, tables.md, degrees.png, speedup.png, schedules.png

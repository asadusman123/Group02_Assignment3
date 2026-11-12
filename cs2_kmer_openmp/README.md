# Case Study 2 - Parallel k-mer Counting (OpenMP)

## Build
    make            # serial, parallel, membench  (gcc -O3 -march=native, -fopenmp)
    make check      # B and C must match the serial checksum for k=8/12, uniform/skewed, p=1,2,4; A must differ

## Run
    ./serial [k] [L] [uniform|skewed] [seed]
    OMP_NUM_THREADS=p OMP_PLACES=cores OMP_PROC_BIND=close ./parallel VERSION [k] [L] [uniform|skewed] [seed] [verify|noverify]
    OMP_NUM_THREADS=p ./membench

Defaults: `k = 12`, `L = 100000000`, `uniform`, `seed = 2026`, `verify`.

| VERSION | what it is |
|---|---|
| `A` | `omp parallel for`, shared histogram, no protection (wrong on purpose: data race) |
| `B` | same, every increment `#pragma omp atomic` |
| `C` | one private histogram per thread, then a parallel merge over bins |
| `N` | wrong chunk boundaries on purpose: cuts the *sequence*, loses (p-1)(k-1) k-mers |
| `fs` / `fspad` | false-sharing demo, k = 1: per-thread counters 32 B apart / padded to 128 B |

## Inputs
Synthetic sequence, L = 10^8 bases, seed 2026.
- `uniform`: A, C, G, T equally likely.
- `skewed`: P(A) = 0.55, P(C) = 0.05, P(G) = 0.05, P(T) = 0.35.

Each base is encoded in 2 bits (A=0, C=1, G=2, T=3), so a k-mer is a 2k-bit integer and the
histogram has 4^k 32-bit bins (k=8: 256 KiB, k=12: 64 MiB).

## Chunk boundaries
The L-k+1 k-mer *start positions* are split into p contiguous chunks. A chunk primes its rolling
window with the k-1 bases at its start, reading into the neighbour's region (read-only). Each
k-mer is therefore counted exactly once, by the chunk that owns its start position.

## Timing
Only counting is timed (for C, this includes allocating and zeroing the private histograms and
the merge). Sequence generation is excluded and printed separately.

## Benchmarks
    ./bench.sh && python3 analyze.py      # ~15 min; results/ gets raw.log, tables.md, speedup.png

## Files
- `kmer_common.h` - sequence generator, encoding, rolling-window kernel, checksum
- `serial.c`, `parallel.c` - the counters
- `membench.c` - sequential read bandwidth and random read-modify-write rate (to interpret Q4)

# Case Study 5 - Distributed Sample Sort (MPI)

## Build
    make            # serial (gcc), parallel and pingpong (mpicc), -O3 -march=native -std=c11
    make check      # uniform/zipf x splitter rule x p = 1,2,4 (N = 1e7): order, count and fingerprint must PASS

## Run
    ./serial [N] [uniform|zipf] [qsort|radix] [seed]
    mpirun --bind-to core -np p ./parallel [N] [uniform|zipf] [ov] [qsort|radix] [tie|notie] [seed]
    mpirun --map-by :OVERSUBSCRIBE -np 4 ./parallel ...           # p > 2 physical cores (Open MPI 5)
    mpirun --bind-to core -np 2 ./pingpong

Defaults: `N = 100000000` 32-bit keys, `uniform`, `ov = 1`, `qsort`, `tie`, `seed = 2026`.

## Inputs
Rank r generates its own N/p keys with seed 2026 * 1000003 + r.
- `uniform`: uniform 32-bit unsigned keys.
- `zipf`: Zipf(s = 1.5) over 2^20 values, so key 0 alone is about 38% of all keys (heavy duplicates).

## Algorithm
1. Local sort (`qsort` as specified; `radix` is an optional LSD radix sort used to study the phase balance).
2. Sampling: `ov*p - 1` regular samples per rank (ov = 1 gives the classic p - 1), `MPI_Gather` to rank 0,
   p - 1 splitters at the global quantiles (j+1)/p, then `MPI_Bcast`.
3. Partition by binary search.
4. `MPI_Alltoall` (counts) + `MPI_Alltoallv` (keys).
5. Pairwise merge of the p received runs.

Splitters are (key, rank, position) triples (`tie`). Runs of equal keys can therefore be divided
between ranks. With `notie` only the key is used.

## Verification
Each rank's data is sorted. The maximum of all lower ranks is <= this rank's minimum (`MPI_Exscan`).
The total count equals N. The multiset fingerprint (sum of hashed keys) is identical before and
after. Phase times are barrier-to-barrier, i.e. the slowest rank.

## Benchmarks
    ./bench.sh && python3 analyze.py     # ~20-25 min: ping-pong, oversampling sweep, timing (qsort + radix)

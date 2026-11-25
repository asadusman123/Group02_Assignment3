# Case Study 5 — Distributed Sample Sort (MPI)

## Problem and why it parallelizes

We sort N = 10⁸ unsigned 32-bit keys (400 MB) distributed over p processes, as if each held one
shard of a larger file. Rank r generates its own N/p keys with seed 2026·1000003 + r. No rank
ever holds more than its own shard plus the keys it receives. There are two input distributions:

- **uniform:** uniform 32-bit keys;
- **zipf:** Zipf(s = 1.5) over 2²⁰ values, so key 0 alone is about 38% of all keys, and keys
  1, 2, 3, … also have long runs.

Sorting is not embarrassingly parallel, but **sample sort** reduces it to independent local
sorts plus one all-to-all exchange. If the p − 1 splitters cut the key space into p buckets of
roughly equal size, rank j can receive bucket j from everyone and sort it independently. The
concatenation of the ranks, in rank order, is then globally sorted.

## Model and decomposition

**Model: MPI** (recommended). The data is distributed by design and the redistribution is a
collective (`MPI_Alltoallv`). **Decomposition: data decomposition, first by shard (input), then
by key range (output)** (figure).

![CS5 pipeline](figures/cs5_decomposition.png)

1. **Local sort.** `qsort`, as the brief specifies. An optional LSD radix sort (`radix`) is used
   only to study how the phase balance changes when local sorting is cheap.
2. **Sampling.** Each rank takes s = ov·p − 1 **regular** samples at local positions
   (i+1)·n/(ov·p). With ov = 1 this is the classic p − 1 samples per rank; ov > 1 is
   oversampling. `MPI_Gather` sends them to rank 0, which sorts the p·s samples and picks splitter
   j at global position (j+1)/p, then `MPI_Bcast`.
3. **Partition.** Binary search of each splitter in the sorted local array gives p bucket
   boundaries.
4. **Redistribution.** `MPI_Alltoall` exchanges the bucket sizes, then `MPI_Alltoallv` the keys.
5. **Merge.** The p received runs are each already sorted, so they are merged pairwise in log₂ p
   passes.

**Duplicate keys.** A sample (and so a splitter) is the triple **(key, rank, position)**,
compared lexicographically. Equal keys are thereby ordered by where they came from, and a long
run of one key can be divided between ranks: when a rank partitions at a splitter with the same
key, its copies go left if its rank is lower than the splitter's, right if higher, and are split
at the sample's position if equal. The `notie` variant compares keys only, which is the textbook
behaviour and the one that breaks on Zipf input (Q1).

## Correctness validation

Every run checks:
1. each rank's final data is sorted;
2. global order: the maximum over all lower ranks ≤ this rank's minimum. We compute it with
   `MPI_Exscan(MAX)`, which also handles ranks that end up empty, and it is equivalent to
   max_i ≤ min_{i+1} for non-empty neighbours;
3. the total count equals N (`MPI_Allreduce`);
4. the **multiset fingerprint**, the sum of a 64-bit hash of every key, is identical before and
   after, so no key was lost, duplicated or changed.

`make check` runs uniform/Zipf × key-only/tie-break × p = 1, 2, 4 (N = 10⁷): **12/12 PASS**.
Every benchmark run (N = 10⁸, 16 timed configurations × 3 rounds, plus 40 oversampling runs)
also reported PASS. The serial baseline verifies order and fingerprint the same way.

## Critical analysis

### Q1 — Load balance after redistribution, and oversampling

Final number of keys per rank (N = 10⁸, mean = N/p):

| p | input | splitter rule | ov = 1 | ov = 256 | max/mean ov=1 → 4 → 16 → 64 → 256 |
|---|---|---|---|---|---|
| 4 | uniform | key + rank + pos | 24 997 591, 25 002 068, 25 003 180, 24 997 161 | identical | 1.000 at every ov |
| 4 | zipf | **key only** | **38 304 375**, 13 546 075, 23 371 255, 24 778 295 | identical | **1.532** at every ov |
| 4 | zipf | key + rank + pos | 25 399 423, 22 601 964, 26 814 618, 25 183 995 | 24 984 383, 25 034 323, 24 973 856, 25 007 438 | 1.073 → 1.040 → 1.007 → 1.008 → **1.001** |
| 2 | zipf | key only | 51 851 675, 48 148 325 | identical | 1.037 at every ov |
| 2 | zipf | key + rank + pos | 50 929 026, 49 070 974 | 50 050 119, 49 949 881 | 1.019 → … → **1.001** |

![CS5 oversampling](../cs5_samplesort_mpi/results/oversampling.png)

- **Uniform input is balanced to 0.01% even with p − 1 samples.** Every rank's shard is an
  independent sample of the same distribution, so its regular samples are already almost exact
  quantiles. Oversampling does not even change the chosen splitters. The counts are identical at
  ov = 1 and 256.
- **Zipf with key-only splitters has a hard floor that no amount of sampling can lower.** All
  38.3 M copies of key 0 (38.3% of N, matching the Zipf weight 1/ζ(1.5) = 0.383) must go to one
  rank, so max/mean ≥ 0.383/0.25 = **1.53** at p = 4 whatever the samples are. Rank 0 does 2.8×
  the work of rank 1.
- **Tie-breaking removes the floor, and oversampling removes the rest.** With (key, rank, pos)
  splitters, a run of equal keys can be cut anywhere. The splitter is the sample at global
  position (j+1)/p of all p·s samples, and the error of that position estimate shrinks with the
  sample count. Inside the long runs of keys 0, 1, 2, … the p·(p−1) samples of ov = 1 are too
  coarse (1.073). With 64× and 256× more samples the cut points land within 0.1% of the ideal
  (1.008, 1.001). Oversampling costs almost nothing: the sampling phase stays at or below 0.02 s
  even at ov = 256.

### Q2 — Phase breakdown

Median of 3 runs, N = 10⁸; phase times are barrier-to-barrier (the slowest rank).

| input, local sort | p | total (s) | local sort | sampling | partition | Alltoall(v) | merge |
|---|---|---|---|---|---|---|---|
| uniform, qsort | 1 | 33.904 | 33.623 | 0.000 | 0.000 | 0.287 | 0.000 |
| uniform, qsort | 2 | 19.973 | **19.198** | 0.000 | 0.000 | 0.246 | 0.635 |
| uniform, qsort | 4 | 19.649 | **18.214** | 0.001 | 0.000 | 0.171 | 0.583 |
| zipf, qsort | 4 | 12.047 | **11.406** | 0.000 | 0.000 | 0.239 | 0.402 |
| uniform, radix | 1 | 2.651 | 2.107 | 0.000 | 0.000 | 0.351 | 0.000 |
| uniform, radix | 2 | 2.598 | **1.534** | 0.000 | 0.000 | 0.301 | 0.763 |
| uniform, radix | 4 | 3.105 | 1.744 | 0.020 | 0.011 | 0.342 | **1.321** |
| zipf, radix | 4 | 1.769 | 0.936 | 0.001 | 0.000 | 0.334 | 0.410 |

![CS5 phases](../cs5_samplesort_mpi/results/phases.png)

- **With qsort, the local sort dominates at every p (93–99% of the time).** `qsort` makes about
  N/p·log₂(N/p) comparisons, each through a function pointer. Sampling and partition are
  negligible: microseconds to 20 ms, because they touch only O(p²) samples and do O(p log n)
  binary searches.
- **Once the local sort is cheap, the dominant phase changes with p.** With radix sort (4 linear
  passes, 10–14× faster than qsort), the O(N) data-movement phases take over. At p = 4,
  **merge + Alltoallv = 1.66 s, almost as much as the 1.74 s local sort** (54% of the total,
  vs 41% at p = 2). The merge grows with p: each rank does log₂ p passes over N/p keys, so per-rank merge
  work is the same at p = 2 and 4 (1 × N/2 vs 2 × N/4). At p = 4, though, two ranks share each
  physical core, so the merge time almost doubles (0.763 → 1.321 s).
- **Zipf is faster to sort than uniform** (qsort: 22.6 vs 30.5 s serial). 38% of the keys are a
  single value, and long runs of equal keys make `qsort` partitions cheaper.

### Q3 — Communication cost model

**Ping-pong** (2 ranks on separate cores, one-way time, best of 3 rounds):

| message | 1 B | 64 B | 1 KiB | **4 KiB** | 64 KiB | 1 MiB | 16 MiB | 64 MiB |
|---|---|---|---|---|---|---|---|---|
| time (µs) | 0.27 | 0.30 | 0.60 | **3.88** | 11.93 | 176.4 | 3 547 | 14 445 |
| GB/s | – | 0.21 | 1.70 | 1.06 | 5.49 | 5.94 | 4.73 | 4.65 |

Fit: **α = 0.29 µs** (mean of messages ≤ 64 B) and **β = 0.2125 ns/B (4.71 GB/s)** (least squares
over 64 KiB–16 MiB). β from the 64 MiB message alone is the same (4.65 GB/s). The jump between
1 KiB (0.60 µs) and 4 KiB (3.88 µs), where bandwidth briefly *falls*, is Open MPI switching from
the eager protocol to a rendezvous handshake for larger messages. That is the same mechanism
behind the overlap result in CS1 Q2.

![CS5 ping-pong](../cs5_samplesort_mpi/results/pingpong.png)

**Prediction vs measurement.** The busiest rank sends 95.4 MiB to others at p = 2 and
71.5–82.4 MiB at p = 4. Model T = (p−1)α + β·bytes:

| input, sort | p | α–β prediction (s) | + local buffer term (s) | corrected prediction (s) | measured (s) |
|---|---|---|---|---|---|
| uniform, qsort | 2 | 0.021 | 0.157 | 0.178 | 0.246 |
| uniform, qsort | 4 | 0.016 | 0.078 | 0.094 | 0.171 |
| zipf, qsort | 2 | 0.021 | 0.160 | 0.181 | 0.218 |
| zipf, qsort | 4 | 0.018 | 0.084 | 0.102 | 0.239 |
| uniform, radix | 2 / 4 | 0.021 / 0.016 | 0.157 / 0.078 | 0.178 / 0.094 | 0.301 / 0.342 |

**The plain α–β model underestimates the measured phase by 10–21×.** The reason is visible at
p = 1, where no byte crosses between ranks, yet the redistribution phase still takes
**0.29–0.35 s**. The phase contains work the model leaves out. The rank allocates a fresh receive
buffer the size of its final data (400 MB at p = 1), and every one of its ≈ 100 000 pages is
page-faulted and zeroed by the kernel the first time `MPI_Alltoallv` writes to it. The rank also
copies its own bucket into that buffer. The ping-pong reuses one warm buffer, so it never pays
this cost. Measured at p = 1, it is 0.31 s / 400 MB ≈ **0.78 ns per received byte**. Adding
0.78 ns × (bytes this rank receives) to the α–β term gives the *corrected* column. It explains
the measurement to within **1.2–1.7× at p = 2**. At p = 4 it is still 1.8–3.6× low, because the
ping-pong measured one transfer at a time with the other core idle. In the all-to-all, every rank
copies and faults pages at the same time over one memory bus, and at p = 4 two ranks share each
core. In short, on a single shared-memory node, "communication" is memory copying and page
management, and its cost is set by the memory system, not by α and β alone.

### Q4 — Comparison with a single-process sort

| input | local sort | serial (s) | p = 2 (s) | p = 4 (s) | speedup p = 2 / p = 4 |
|---|---|---|---|---|---|
| uniform | qsort | 30.484 | 19.973 | 19.649 | **1.53 / 1.55** |
| zipf | qsort | 22.563 | 15.546 | 12.047 | 1.45 / **1.87** |
| uniform | radix | 2.219 | 2.598 | 3.105 | **0.85 / 0.71** (slower) |
| zipf | radix | 2.176 | 1.863 | 1.769 | 1.17 / 1.23 |

(On minimum times, uniform qsort at p = 4 reaches 23.83/11.46 = 2.08. As in the other case
studies, this laptop's medians carry thermal noise: the serial qsort ranged from 23.8 to 30.5 s.)

**Why the speedup is sub-linear even with perfect balance (uniform, max/mean = 1.000):**

1. **The computation itself would scale slightly *better* than linearly.** Each rank sorts N/p
   keys: N log N / ((N/p) log(N/p)) = p · log N / log(N/p), which is 2.08 at p = 2, not 2. But
   this laptop has two physical cores that slow down when both are busy (CS3). The qsort phase
   went from 30.5 to 19.2 s, a 1.59× gain, and p = 4 adds only hyper-threads (18.2 s).
2. **Sample sort adds O(N) work the serial sort never does.** Every key is copied into a fresh
   buffer in the redistribution (≈ 0.2–0.35 s including page faults) and copied again log₂ p
   times by the merge (0.4–1.3 s). With qsort this overhead is 4–5% of the runtime. With radix
   sort, whose total cost is itself only a few linear passes, the overhead is as large as the
   sort, so sample sort is **slower than the serial radix sort** (0.85×, 0.71×). That is the
   O(N log N) computation vs O(N) data movement argument made concrete: the less computation per
   key, the larger the fraction of time spent moving data, and data movement does not speed up
   with more processes on one memory bus.
3. **All of it shares one memory system.** On a cluster, each node's Alltoallv traffic and merge
   would use its own memory bandwidth. Here they compete, like the stencil in CS1.

*References:* H. Shi, J. Schaeffer, "Parallel sorting by regular sampling," JPDC 14(4), 1992.
G. E. Blelloch et al., "A comparison of sorting algorithms for the Connection Machine CM-2,"
SPAA 1991 (sample sort). MPI 4.0 standard (`MPI_Alltoallv`, `MPI_Exscan`). Open MPI
documentation (eager/rendezvous protocols). R. W. Hockney, "The communication challenge for MPP:
Intel Paragon and Meiko CS-2," Parallel Computing 20(3), 1994 (α–β model).

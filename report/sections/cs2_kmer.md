# Case Study 2 — Parallel k-mer Counting (OpenMP)

## Problem and why it parallelizes

We count every length-k substring (k-mer) of a DNA sequence. Each base is encoded in 2 bits
(A=0, C=1, G=2, T=3), so a k-mer is a 2k-bit integer and is used directly as an index into a
histogram of 4^k counters. The input is a synthetic sequence of L = 10⁸ bases generated with a
fixed seed (2026), in two variants: **uniform** (each base equally likely) and **skewed**
(P(A) = 0.55, P(C) = P(G) = 0.05, P(T) = 0.35, so a few A/T-rich k-mers dominate). We use k = 8
(65 536 bins = 256 KiB) and k = 12 (16.8 M bins = 64 MiB).

The L − k + 1 window positions are independent reads of a shared, read-only sequence. The only
dependency is that two positions holding the same k-mer increment the same counter, which is a
classic histogram reduction.

## Model and decomposition

**Model: OpenMP** (recommended). The input is a single in-memory array, and the interesting
questions (races, atomics, private copies, false sharing) are all shared-memory questions.

**Decomposition: data decomposition over k-mer start positions** (figure). The positions
0 … L − k are split into p contiguous chunks, one per iteration of an
`omp parallel for schedule(static)` loop. Each chunk keeps a rolling 2k-bit window, so it reads
each base once. It primes the window with the k − 1 bases at its start, and the last windows of
chunk t read the first k − 1 bases of chunk t + 1 (read-only). Every k-mer is therefore counted
exactly once, by the chunk that owns its **start** position.

![CS2 decomposition](figures/cs2_decomposition.png)

Thread placement: `OMP_PLACES=cores OMP_PROC_BIND=close`. Inside the WSL2 virtual machine,
logical CPUs 0/1 share core 0 and 2/3 share core 1 (`lscpu -e`). At p = 2 the threads are
therefore on different (virtual) cores, and at p = 4 two hyper-threads share each core. Hyper-V
decides which host processors the virtual CPUs actually run on (see CS3, Q3).

## Versions and key implementation decisions

| version | increment | purpose |
|---|---|---|
| serial | `hist[code]++` | baseline |
| A | `hist[code]++` on a shared histogram | wrong on purpose: data race |
| B | `#pragma omp atomic` on a shared histogram | correct, synchronizes every increment |
| C | `mine[code]++` on a private histogram per thread, then a parallel merge where each thread sums a contiguous range of bins across all p copies | correct, no synchronization while counting |
| N | like C, but cuts the *sequence* into p disjoint pieces | wrong on purpose: shows why boundary handling matters |

- Counters are `uint32_t`, enough for L ≤ 4·10⁹.
- In C, each thread allocates and zeroes its own private histogram inside the parallel region.
  First touch places its pages for that thread, and the allocation, zeroing and merge are all
  inside the timed region.
- The same `COUNT_RANGE` macro (rolling window over a range of start positions) is used by every
  version. Only the increment differs.

## Correctness validation

Every parallel histogram is compared bin by bin with a serial histogram of the same sequence
(`verify`). Totals are compared with L − k + 1. Results at L = 10⁸:

| dist | k | p | A: k-mers lost | A: wrong bins | B | C | N: k-mers lost |
|---|---|---|---|---|---|---|---|
| uniform | 8 | 2 / 4 | 25 013 / 32 956 | 20 748 / 25 905 | PASS | PASS | 7 / 21 |
| uniform | 12 | 2 / 4 | 4 / 7 | 4 / 7 | PASS | PASS | 11 / 33 |
| skewed | 8 | 2 / 4 | 1 452 400 / 2 017 126 | 5 434 / 6 035 | PASS | PASS | 7 / 21 |
| skewed | 12 | 2 / 4 | 11 644 / 16 195 | 3 432 / 4 185 | PASS | PASS | 11 / 33 |

B and C match the serial histogram exactly (0 wrong bins) in all 16 configurations. `make check`
repeats this at L = 2·10⁷ for p = 1, 2, 4. Version N loses exactly (p − 1)(k − 1) k-mers, the
windows that straddle a cut, which confirms that our start-position decomposition is what makes
B and C exact.

## Speedup and efficiency

Seven interleaved rounds, L = 10⁸, laptop in its default power state. The table shows medians
(the brief's statistic). Min-based speedups are in `results/tables.md`.

| dist, k | serial (s) | B p=1 / 2 / 4 speedup | C p=1 / 2 / 4 speedup | best throughput |
|---|---|---|---|---|
| uniform, 8 | 0.264 | 0.24 / 0.25 / 0.31 | 1.18 / 1.59 / **1.70** | C p=4: 644 M k-mers/s |
| uniform, 12 | 3.016 | 0.99 / 1.45 / **1.73** | 0.92 / 1.04 / 0.95 | B p=4: 57 M k-mers/s |
| skewed, 8 | 0.170 | 0.19 / 0.17 / 0.18 | 0.97 / 1.21 / **1.41** | C p=4: 827 M k-mers/s |
| skewed, 12 | 1.317 | 0.61 / 0.89 / **1.09** | 0.91 / 0.96 / 0.91 | B p=4: 83 M k-mers/s |

![CS2 speedup](../cs2_kmer_openmp/results/speedup.png)

(C at p = 1 is "faster" than serial for uniform k = 8, 1.18. That is run-to-run noise: the serial
median of 0.264 s is well above its own minimum of 0.190 s, and on minimum times C p = 1 is 0.95.)

## Critical analysis

### Q1 — Correctness of Version A

`hist[code]++` compiles to three instructions: *load* the counter, *add 1*, *store* it back. If
two threads load the same counter before either stores, both store old + 1 and one increment is
lost. This is a **lost-update data race** on a read-modify-write of shared memory. The totals
prove it: for uniform k = 8 at p = 2 the program counted 99 974 980 k-mers instead of
L − k + 1 = 99 999 993, so **25 013 increments were lost** and 20 748 bins are wrong.

How much is lost depends on how often two threads hit the **same counter at nearly the same
time**:

- **Uniform k = 12:** 16.8 M bins, so a collision is rare. Only **4–7** increments were lost per
  10⁸, and the bug would easily pass a casual test. In a smaller test (L = 5·10⁶, p = 4) we
  observed zero lost updates, which makes the point: a race can produce correct output by chance.
- **Uniform k = 8:** 65 536 bins → 25–33 thousand lost.
- **Skewed k = 8:** the k-mer AAAAAAAA alone has probability 0.55⁸ ≈ 0.8%, so about 840 000
  increments hit one counter. Threads collide constantly, and **2.0 million** increments are lost
  at p = 4 (2% of the total).

More threads means more collisions, so A gets more wrong as p grows, in every case.

### Q2 — Atomic (B) vs private (C), and the crossover

| k | p | B (s) | C (s) | B / C | extra memory of C (p × 4^k × 4 B) |
|---|---|---|---|---|---|
| 8 | 1 | 1.119 | 0.225 | 4.98 | 256 KiB |
| 8 | 2 | 1.045 | 0.166 | 6.31 | 512 KiB |
| 8 | 4 | 0.844 | 0.155 | 5.44 | 1 MiB |
| 12 | 1 | 3.056 | 3.287 | 0.93 | 64 MiB |
| 12 | 2 | 2.079 | 2.900 | 0.72 | 128 MiB |
| 12 | 4 | 1.740 | 3.172 | 0.55 | 256 MiB |

**For k = 8, private wins by 5–6×.** A 256 KiB histogram fits in each core's 256 KiB L2, so C's
increments are plain cache-resident adds with no communication between cores. An atomic increment
is a `lock`-prefixed read-modify-write. Even uncontended, B at p = 1 costs 1.119 s vs 0.264 s
serial, about **8.5 ns extra per increment**. With two cores, the 4 096 cache lines of the shared
histogram (16 counters per line) also move back and forth between the cores' caches, so B never
gets faster than serial (S ≤ 0.31). On the skewed input B gets *slower* with more threads
(0.87 → 0.99 → 0.93 s), because most increments go to the same few hot lines.

**For k = 12, atomic wins, and the gap grows with p.** A 64 MiB histogram is 20× larger than the
3 MiB L3, so nearly every increment is a DRAM miss in either version. The atomic's extra cost is
hidden behind a miss of tens of nanoseconds: B at p = 1 equals serial (0.99), and B scales to 1.73
at p = 4. C instead pays for replication:
- each extra thread adds another 64 MiB table to allocate, zero and page-fault in;
- the merge reads p × 64 MiB, taking 57–65 ms;
- most importantly, the counting phase now touches p × 64 MiB of randomly accessed memory instead
  of 64 MiB. Our `membench` shows random read-modify-writes on per-thread 64 MiB tables do not
  scale at all on this machine: **35 / 37 / 34 M updates/s at p = 1 / 2 / 4**. That is the same
  rate as the serial counter (33 M k-mers/s), and C stays flat at about 32 M/s.

B on a single shared 64 MiB table reaches 57 M/s, beyond that per-thread ceiling. The most likely
reason is the smaller footprint: one table needs 4× fewer page translations than four, so fewer
TLB misses and page walks. We did not measure TLB misses directly.

**The crossover rule:** private copies win while p × 4^k × 4 B fits in cache (1 MiB at k = 8,
p = 4). Atomics win once the histogram is DRAM-resident, because replication multiplies an
already memory-bound working set. **Extra memory of C** = p × 4^k × 4 B: our code keeps p
private copies in addition to the final histogram. A variant that reuses thread 0's copy as the
result would need (p − 1) × 4^k × 4 B. The extra is 1 MiB at k = 8 but **256 MiB** at k = 12,
p = 4, more than 2.5× the 100 MB input sequence itself.

### Q3 — False sharing

We built the classic case with k = 1: each thread keeps its 4 counters (one per base) in one
shared array. *Unpadded*, a thread's counters occupy 32 bytes, so threads 0 and 1 share a 64-byte
cache line. *Padded*, each thread's block is 128 bytes. Counters are written through a `volatile`
pointer, so every increment really goes to memory, as it would if other threads could read the
counters.

| p | unpadded (s) | padded (s) | unpadded / padded | unpadded speedup vs p=1 | padded speedup vs p=1 |
|---|---|---|---|---|---|
| 1 | 0.126 | 0.115 | 1.10 | 1.00 | 1.00 |
| 2 | **0.301** | 0.091 | **3.31** | **0.42** | 1.26 |
| 4 | 0.207 | 0.078 | 2.65 | 0.61 | 1.47 |

At p = 2 the two threads run on different cores and never touch each other's counters.
But each write needs the whole 64-byte line in Modified state in the writer's L1, so the line
bounces between the two cores on almost every increment. The result: **2 threads are 2.4× slower
than 1**, and padding makes them 3.3× faster.

At p = 4, threads 0 and 1 share core 0 and threads 2 and 3 share core 1, so each 64-byte line is
shared only by hyper-threads of the same core. Even so, unpadded remains 2.65× slower. We padded
to 128 bytes rather than 64 because Intel's L2 spatial prefetcher fetches lines in 128-byte
pairs, so neighbouring lines still interfere across cores. Writes to one line from two
hyper-threads can also cause memory-ordering pipeline flushes. We cannot separate these two
effects without hardware counters. The padded speedups are modest (1.26, 1.47) because the whole
k = 1 run is only about 0.1 s, so thread start-up and the multi-core turbo drop matter.

### Q4 — Memory-bound or compute-bound?

**Bytes per k-mer.** Every k-mer streams 1 new byte of sequence; the window is rolling, so the
other k − 1 bases are already in a register. It also does a read-modify-write of one 4-byte
counter:

- **k = 8:** the counter is in L1/L2. DRAM traffic is only the sequence stream: **1 B per
  k-mer**. At the best rate of 644 M k-mers/s (C, p = 4) that is 0.64 GB/s, about 4% of the
  measured read bandwidth (16.6 GB/s, `membench`). k = 8 is therefore **compute/cache-bound**,
  limited by the per-k-mer instruction chain (load base → table lookup → shift/or/mask →
  increment in cache). Scaling flattens between p = 2 and 4 (604 → 644 M/s) because the two
  hyper-threads of a core share its 256 KiB L2, and two private 256 KiB histograms no longer fit.
  `membench` shows the same flattening for 256 KiB random updates (517 → 594 M/s).
- **k = 12:** each increment misses all caches. It reads a 64-byte line and later writes back
  the dirty line, about **129 B of DRAM traffic per k-mer**. The serial rate of 33 M k-mers/s
  moves 4.3 GB/s, and the best (B, p = 4, 57 M/s) moves 7.4 GB/s. That is well below the 16.6 GB/s
  sequential bandwidth, so we are not bandwidth-bound either. We are **memory-latency-bound**:
  throughput is set by how many random misses (and TLB misses: 64 MiB is 16 384 pages of 4 KiB)
  the core can have in flight. `membench`'s random-update test on a 64 MiB table runs at
  35 M updates/s, the same as the serial counter, and it does not improve with more threads.
  That is where our k = 12 scaling flattens.

*References:* OpenMP Architecture Review Board, *OpenMP API Specification 5.2* (2021), sections
on `atomic`, `reduction` and thread affinity. Intel, *64 and IA-32 Architectures Optimization
Reference Manual* (spatial prefetcher, false sharing).

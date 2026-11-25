# Case Study 4 — Parallel PageRank (OpenMP)

## Problem and why it parallelizes

PageRank iterates r_v(t+1) = (1 − d)/n + d·(Σ_{u→v} r_u/outdeg(u) + D(t)/n), with d = 0.85,
until ‖r(t+1) − r(t)‖₁ < ε = 10⁻⁹ (24 iterations on our graph). D(t) is the total rank of the
**dangling** vertices (out-degree 0), which is spread uniformly over all n vertices, so the ranks
always sum to 1. Within one iteration every r_v(t+1) depends only on the previous vector r(t),
so all n vertex updates are independent.

**Input.** A synthetic power-law graph from an **R-MAT** generator (Chakrabarti et al., 2004) with
the Graph500 parameters a, b, c, d = 0.57, 0.19, 0.19, 0.05, scale 20 and edge factor 16, seed
2026. Self-loops and duplicate edges are removed, leaving **n = 1 048 576 vertices and
m = 16 085 679 edges** (average degree 15.3; maximum in-degree 39 615, maximum out-degree
39 581). 47.9% of the vertices are dangling and 38.3% are isolated. The graph is stored in CSR in
both directions: in-edges for pull, out-edges for push. 32-bit indices give 131 MiB in total.

![CS4 degree distribution](../cs4_pagerank_openmp/results/degrees.png)

The log–log degree distribution is heavy-tailed. Most vertices have degree 0–20, and a few have
tens of thousands. The visible "comb" of bumps is a known property of R-MAT without noise: a
vertex's expected degree is set by how many 0- and 1-bits its id has, so vertices group by
popcount.

## Model and decomposition

**Model: OpenMP** (recommended). The graph fits in memory, and the questions (load balance,
schedules, atomics) are about shared memory.

**Decomposition: data decomposition over vertices** (figure).

- **Pull:** threads own **destination** vertices v and gather the contributions r_u/outdeg(u) of
  v's in-neighbours. Shared data is only read, and each thread writes only its own rn[v], so no
  synchronization is needed.
- **Push:** threads own **source** vertices u and scatter r_u/outdeg(u) into next[v] for every
  out-neighbour. Two threads can update the same v, so each update is `#pragma omp atomic`.

![CS4 pull vs push](figures/cs4_decomposition.png)

Each iteration is one parallel region with three steps:
1. a static loop computes contrib[u] = r_u/outdeg(u) and the dangling mass D (a reduction);
2. the **edge loop** uses `schedule(runtime)`, so `OMP_SCHEDULE` selects the schedule we study,
   and also reduces the L1 change;
3. for push, a static loop applies the teleport and dangling terms.

The edge loop uses `nowait`, and each thread records its own loop time and edge count. Their
ratios give the load-imbalance ratio R.

## Correctness validation

The serial pull version writes its final ranks to a file. Every parallel run computes the L1
distance to them. **Tolerance: ‖r_par − r_ser‖₁ < 10⁻⁹**; the ranks sum to 1, and ε itself is
10⁻⁹. The only expected difference is floating-point summation order: the D and L1 reductions,
and push's update order.

| version | p = 1 | p = 2 | p = 4 |
|---|---|---|---|
| pull | 0 (bit-identical), 24 it. | 5.7·10⁻¹³ PASS, 24 it. | 9.1·10⁻¹³ PASS, 24 it. |
| push (atomic) | 0, 24 it. | 5.7·10⁻¹³ PASS | 9.1·10⁻¹³ PASS |
| push **without** atomic | 0, 24 it. | **2.2·10⁻³ FAIL**, never converged (200 it.) | **3.1·10⁻³ FAIL**, 200 it. |

`make check` also verifies pull and push for p = 1, 2, 4 under static, dynamic,256 and guided.
The push without atomics is included deliberately: its lost updates are large enough that the
iteration never reaches ε.

## Speedup and efficiency

Median of 5 interleaved runs; serial pull: 2.713 s (minimum 2.329 s) = 142 M edges/s.

| version | schedule | p = 1 | p = 2 | p = 4 (HT) | R at p = 4 |
|---|---|---|---|---|---|
| pull | static | 2.502 s | 2.222 s (S 1.22) | 1.904 s (S 1.42) | 1.783 |
| pull | dynamic,256 | 2.531 s | 1.998 s (S **1.36**, min-based 1.75) | 1.793 s (S **1.51**) | 1.011 |
| pull | guided | 2.391 s | 2.043 s (S 1.33) | 1.903 s (S 1.43) | 1.118 |
| push | static | 8.602 s | 7.806 s (S 0.35) | 6.144 s (S 0.44) | 1.795 |
| push | dynamic,256 | 7.100 s | 4.918 s (S 0.55) | 4.823 s (S 0.56) | 1.001 |

![CS4 speedup](../cs4_pagerank_openmp/results/speedup.png)

The best configuration, pull with dynamic,256 at p = 4, processes **215 M edges/s** (median).
On minimum times, p = 2 reaches 1.75. The gap between median and minimum (serial: 2.71 vs 2.33 s)
is this laptop's thermal noise, discussed in CS3.

## Critical analysis

### Q1 — Irregularity: equal vertex counts do not mean equal work

In pull, the work for vertex v is its **in-degree**, which ranges from 0 (38% of vertices are
isolated) to 39 615, i.e. 2 600 times the average. With `schedule(static)`, each thread receives
n/p consecutive vertices but very different numbers of edges:

| p | static: edges max/mean | static: thread time max/mean R | dynamic,256: R |
|---|---|---|---|
| 2 | **1.510** | 1.385 | 1.000 |
| 4 | **2.278** | 1.783 | 1.011 |

On R-MAT the imbalance is **predictable from the vertex ids**. Each bit of a destination id is 0
with probability a + c = 0.76 and 1 with probability b + d = 0.24. Static at p = 2 gives thread 0
every v with top bit 0, so it expects 76% of the edges: max/mean = 0.76/0.5 = **1.52** (measured
1.510). At p = 4, thread 0 gets top bits 00: 0.76² = 57.8% of the edges, so max/mean = 2.31
(measured 2.278). The same arithmetic explains why **`static,c` does not help either**. With c a
power of two, round-robin chunk ownership is decided by bit log₂c of the id, which is again
76/24-skewed. We measured 1.511 and 2.279 for c = 64 and 1024, exactly static's values. The
thread time ratio R is lower than the edge ratio because every vertex also has a fixed per-vertex
cost (offsets, writing rn[v], the L1 change), which is equal across threads.

### Q2 — Scheduling: imbalance vs overhead

Pull, median time in seconds (full table, including R and edge shares, in
`results/tables.md`):

| schedule | p = 2 | p = 4 |
|---|---|---|
| static | 2.098 | 1.835 |
| static,64 / static,1024 | 2.093 / 2.020 | 1.876 / 2.028 |
| dynamic,1 | **2.785** | **2.619** |
| dynamic,16 | 1.891 | 1.705 |
| dynamic,64 | 1.625 | 1.700 |
| dynamic,256 | 1.922 | 1.618 |
| dynamic,1024 | 1.727 | **1.604** |
| dynamic,4096 | 1.650 | 1.995 |
| dynamic,16384 | **1.535** | 1.731 |
| guided / guided,64 / guided,256 | 1.916 / 1.935 / 1.933 | 2.070 / 2.122 / 1.973 |

![CS4 schedules](../cs4_pagerank_openmp/results/schedules.png)

- **Static and static,c** keep the imbalance from Q1. The thread holding the low ids finishes
  last, and the others wait.
- **dynamic,1** fixes the imbalance (R = 1.003), but it is the slowest schedule: 2.62 s at p = 4.
  Every vertex costs one grab from a shared iteration counter, an atomic fetch-and-add on a cache
  line that bounces between cores. That is 24 × 1 048 576 ≈ 25 M grabs, and the extra ≈ 1.0 s
  over dynamic,1024 works out to **≈ 40 ns per grab**, against an average of ≈ 1.7 ns of useful
  work per vertex.
- **dynamic, c = 16 … 16384** brings R to 1.000–1.011 with negligible overhead. At c = 1024 there
  are only about 1 000 grabs per iteration. Differences between these chunk sizes (1.53–2.00 s)
  are within the run-to-run noise of this machine. The best measured values, dynamic,1024 at
  p = 4 and dynamic,16384 at p = 2, are not significantly better than their neighbours.
- **guided** hands out large chunks first and smaller ones later. On this graph the first, large
  chunks contain the heaviest vertices, so R stays at 1.12 at p = 4, and guided was no faster than
  static in our runs.

**Best choice: `dynamic` with c ≈ 256–1024.** Chunks are small compared with n/p, so the work of
the last chunks evens out, and large enough that n/c grabs per iteration cost almost nothing. The
trade-off is clearest at the extremes: c = 1 pays maximum overhead with perfect balance, and
static pays no overhead with maximum imbalance.

### Q3 — Pull vs push: the cost of atomics

| version (p = 1, static) | time (s) | ns per edge per iteration |
|---|---|---|
| pull (random reads) | 2.502 | 6.48 |
| push without atomic (random writes, single thread) | 2.829 | 7.33 |
| push with `omp atomic` (random atomic writes) | 6.979 | **18.08** |

At p = 1 there is no contention, and the atomic still costs **10.75 ns per edge, +147%** on top of
the same scatter without it. `next[v] += c` on a `double` has no single x86 instruction. GCC
compiles it to a load–add–`lock cmpxchg` retry loop (we checked the assembly: four
`lock cmpxchgq`). The `lock` prefix is a full memory barrier. The CPU cannot start the next
scattered update's cache miss until the current one has completed, so misses that a plain
scatter would overlap are paid one after another.

With more threads, push improves only to 4.82 s at p = 4 (dynamic,256), still **2.7× slower than
pull** (1.79 s). The **access patterns** explain why:
- **Pull** streams in_src sequentially and does *random reads* of contrib[u]. Many reads can be
  in flight at once, and a cache line can sit in several cores' caches in Shared state at the
  same time.
- **Push** streams out_dst and does *random read-modify-writes* of next[v]. Every update needs the
  line in **exclusive** state, which means a read-for-ownership and invalidating any other core's
  copy. Hot destinations (in-degree up to 39 615) are updated by all threads, so their lines
  ping-pong between cores, and failed `cmpxchg` attempts retry. Push also has an extra pass per
  iteration (clearing and finishing next[]).

### Q4 — Memory behaviour

**Bytes per edge per iteration (pull).** Each edge reads one 4-byte index from in_src
(sequential) and gathers one 8-byte contrib[u] (random). The gather costs no DRAM traffic if the
line is cached, and a full 64-byte line if it misses. Per vertex the code reads in_off,
out_off and r twice, and writes contrib, rn and next, counting write-allocate. Our pull code also
clears next[], which it does not need (16 B/vertex we would remove in a tuned version). That is
about 88 B per vertex, or **≈ 5.7 B per edge** amortized over 15.3 edges per vertex. In total:

DRAM traffic ≈ **9.7 B/edge** if every gather hits in cache, up to **≈ 74 B/edge** if every
gather misses.

**Achieved:** 215 M edges/s (median, p = 4); 290 M edges/s best case (minimum time at p = 2).
At 215 M edges/s this is **2.1 GB/s** if all gathers hit and 15.9 GB/s if all miss. The machine's
measured bandwidth is 13.4 GB/s on one core and 16.6 GB/s on all cores (`membench`, CS2).

**How close to the bandwidth limit?** Not close. The all-miss case would put us at the bandwidth
roof, but the per-edge time rules it out. A random access to DRAM on this machine sustains only
about 35 M accesses/s (`membench`, 64 MiB table, CS2), about 29 ns each. Serial pull spends only
**6.5 ns per edge**, so most gathers must hit in cache. R-MAT concentrates edges on a minority of
popular low-id sources, and in-lists are sorted by source, so their contrib values stay cached.
A rough estimate is a 10–15% miss rate, with ≈ 3 ns per hit and ≈ 29 ns per miss (an estimate;
we have no hardware counters under WSL2). That gives 9.7 + 0.13 × 64 ≈ 18 B/edge, or **≈ 4 GB/s,
about a quarter of the bandwidth**. PageRank here is limited by the **latency of the
cache-missing gathers** and by the 3 MiB shared L3, which is smaller than the 8 MiB contrib
array. It is not limited by bandwidth. This is why a second core helps (1.36× median, 1.75× best)
but hyper-threading adds little: both hyper-threads share one core's L1/L2 and its limited number
of outstanding misses.

*References:* D. Chakrabarti, Y. Zhan, C. Faloutsos, "R-MAT: A recursive model for graph mining,"
SIAM SDM 2004. Graph500 benchmark specification (R-MAT parameters). L. Page, S. Brin, R. Motwani,
T. Winograd, "The PageRank citation ranking: bringing order to the web," Stanford tech. report,
1999. OpenMP API 5.2 (`schedule` clause, `atomic`).

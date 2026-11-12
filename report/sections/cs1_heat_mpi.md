# Case Study 1 — Heat Diffusion Simulation (MPI)

## Problem and why it parallelizes

We simulate 2-D heat diffusion on an N × N grid with the explicit 5-point stencil
u′ᵢⱼ = uᵢⱼ + α(uᵢ₋₁ⱼ + uᵢ₊₁ⱼ + uᵢⱼ₋₁ + uᵢⱼ₊₁ − 4uᵢⱼ), α = 0.2, for T = 500 steps. Two grids are
swapped every step. The outer ring is a fixed boundary (top 100, left 75, right 50, bottom 0) and
the interior starts at 0. Every K steps we compute the maximum change of any cell over that step
as a convergence metric.

Within one step, every cell's new value depends only on the **previous** grid, so all cells can be
updated in parallel. The only coupling is between neighbouring cells. If the grid is split into
blocks, each block needs one layer of its neighbours' cells (the halo) per step, plus one global
reduction every K steps.

## Model and decomposition

**Model: MPI** (recommended). The grid is split into blocks that each process owns privately,
and the only communication is explicit halo exchange and an `MPI_Allreduce`.

**Decomposition: block data decomposition on a Cartesian process topology** (figure).
`MPI_Cart_create` builds a p₀ × p₁ grid of processes, and `MPI_Cart_shift` gives each rank its up,
down, left and right neighbours (`MPI_PROC_NULL` at the domain edge). With dims p × 1 this is the
**1-D row-block** decomposition. With dims from `MPI_Dims_create(p, 2)` it is the **2-D block**
decomposition. One code path serves both. Each rank stores its block plus a one-cell ghost ring.

![CS1 decomposition](figures/cs1_decomposition.png)

On our 2-core machine `MPI_Dims_create` gives 1 × 1 and 2 × 1 for p = 1 and 2, so the two
decompositions differ only at p = 4 (4 × 1 vs 2 × 2).

## Key implementation decisions

- **Halo datatypes.** Rows are contiguous, so they are sent as `MPI_Type_contiguous`. Columns are
  strided, so they use an `MPI_Type_vector` with stride (nc + 2) and no manual packing.
- **Three halo modes.**
  - `blocking`: four `MPI_Sendrecv` calls (up, down, left, right), then update every owned cell.
  - `nonblocking`: post 4 `MPI_Irecv` + 4 `MPI_Isend`, update the *inner* cells that need no
    ghost values while messages are in flight, `MPI_Waitall`, then update the one-cell outer frame.
  - `nbtest`: like nonblocking, but calls `MPI_Testall` every 16 rows of the inner update so the
    MPI library can progress the transfers (see Q2).
- **Convergence.** On steps where t mod K = 0, the update loop also tracks max |u′ − u|, followed
  by `MPI_Allreduce(MAX)`. On other steps it runs a plain update loop.
- **Bit-identical arithmetic.** All versions use one `STENCIL()` macro, and we compile with
  `-std=c11`, which turns off FMA contraction. Every version therefore performs exactly the same
  floating-point operations per cell.
- **Timing.** `MPI_Barrier` + `MPI_Wtime` around the time loop, maximum over ranks. Per-phase
  times (compute, halo communication or wait, allreduce) are also accumulated, maximum over
  ranks.
- **Output for verification.** The final grid is written with MPI-IO (`MPI_File_write_all` with a
  subarray file view), giving the same raw file format as the serial program.

## Correctness validation

The final grid of every variant was compared **byte for byte** with the serial program's output
(`cmp`), at N = 1024, T = 500, K = 50: **18 of 18 runs are bit-identical** (1-D/2-D × blocking,
nonblocking, nbtest × p = 1, 2, 4). `make check` repeats this at N = 512, T = 200, K = 10. The
global max-change metric also matches serial exactly (6.336026·10⁻² at N = 1024). The only
difference is the printed checksum (the sum of all cells), in its last digits, because the
parallel version adds the per-rank partial sums in a different order.

## Speedup and efficiency

Medians of 5 runs (N = 1024) and 3 runs (N = 4096), T = 500, K = 50, interleaved. p ≤ 2:
`mpirun --bind-to core`. p = 4: two ranks per physical core (`--map-by :OVERSUBSCRIBE`).

| N | dims | mode | p | time (s) | speedup | efficiency | compute (s) | halo comm/wait (s) |
|---|---|---|---|---|---|---|---|---|
| 1024 | serial | – | 1 | 1.050 | 1.00 | 1.00 | 1.050 | – |
| 1024 | 2×1 | blocking | 2 | 0.924 | 1.14 | 0.57 | 0.836 | 0.089 |
| 1024 | 2×1 | nonblocking | 2 | 0.836 | 1.26 | 0.63 | 0.783 | 0.063 |
| 1024 | 2×1 | nbtest | 2 | 0.818 | **1.28** | 0.64 | 0.781 | 0.041 |
| 1024 | 2×2 | nonblocking | 4 | 1.829 | 0.57 | 0.14 | 0.588 | 1.280 |
| 1024 | 4×1 | nonblocking | 4 | 1.224 | 0.86 | 0.21 | 0.618 | 0.674 |
| 4096 | serial | – | 1 | 13.792 | 1.00 | 1.00 | 13.792 | – |
| 4096 | 2×1 | blocking | 2 | 12.963 | 1.06 | 0.53 | 12.505 | 0.434 |
| 4096 | 2×1 | nonblocking | 2 | 13.129 | 1.05 | 0.53 | 12.827 | 0.366 |
| 4096 | 2×2 | nonblocking | 4 | 12.916 | 1.07 | 0.27 | 11.011 | 1.888 |
| 4096 | 4×1 | nbtest | 4 | 12.701 | **1.09** | 0.27 | 12.273 | 0.580 |

The full table, with minimum times and every mode, is in `cs1_heat_mpi/results/tables.md`. The
serial N = 1024 median (1.050 s) sits above its own minimum (0.875 s). On minimum times, the
p = 2 speedup at N = 1024 is 1.06–1.09.

![CS1 scaling](../cs1_heat_mpi/results/scaling.png)

![CS1 phase breakdown](../cs1_heat_mpi/results/phases.png)

**Weak scaling** (2-D, nonblocking, N_p = 2048·√p, about 4.19 M cells per rank):

| p | N_p | T_p (s) | weak efficiency T₁/T_p |
|---|---|---|---|
| 1 | 2048 | 3.858 | 1.00 |
| 2 | 2896 | 7.058 | 0.55 |
| 4 (oversubscribed) | 4096 | 19.196 | 0.20 |

**The dominant result: the stencil is memory-bandwidth-bound, so a second core barely helps.**
Look at the *compute* column, which contains no communication at all: at N = 4096, each of 2 ranks
spends 12.5 s computing half the grid, almost as long as serial takes for the whole grid
(13.8 s). Each cell update streams the old value in and the new value out, about **16–24 bytes
per cell** (8 B read, 8 B write, plus 8 B if the write first reads the line into cache), while
the neighbour values come from cache. Serial performs 4094² × 500 = 8.38·10⁹ updates in 13.8 s,
i.e. 608 M updates/s, which is **9.7–14.6 GB/s**. Our `membench` probe (CS2) measured this
machine's sequential bandwidth at **13.4 GB/s with one core and 16.4–16.6 GB/s with two or
more**. Serial already uses nearly all of it. The bandwidth bound for the whole grid at
24 B/cell is 8.38·10⁹ × 24 B / 16.4 GB/s ≈ **12.2 s**, whatever p is. The measured compute times
for p = 2 and 4 are 11.0–12.8 s. The best possible speedup is about 16.6 / 13.4 ≈ 1.24, and we
measured up to 1.09 at N = 4096 and 1.28 at N = 1024.

## Critical analysis

### Q1 — Surface-to-volume

Per rank and step, the work is N²/p cell updates. The halo received by an interior rank is:

- **1-D rows:** 2 rows of N values = **2N**, independent of p, so halo/work = 2p/N.
- **2-D blocks:** 4 edges of N/√p values = **4N/√p**, so halo/work = 4√p/N.

2-D sends less data when 4N/√p < 2N, i.e. **√p > 2, p > 4**. At p = 4 the interior-block volumes
are equal (2N = 4N/2). With our grid sizes, every 2 × 2 block is a corner block with only two
neighbours, so it actually receives N values, against 2N for the middle blocks of 4 × 1. Measured
values per rank per step, maximum rank: 1 024 (2 × 2) vs 2 048 (4 × 1) at N = 1024, and 4 096 vs
8 192 at N = 4096. The ratio grows as √p/2: 2-D halves the halo at p = 16 and quarters it at
p = 64.

Counting latency too, with α per message and β per byte: a 1-D rank sends 2 messages and a 2-D
rank sends 4, so T₁D = 2α + 16Nβ and T₂D = 4α + 32(N/√p)β. 2-D wins only when the bandwidth saving
outweighs two extra message latencies, 16Nβ(1 − 2/√p) > 2α. That requires p > 4 and large N.
On our machine this crossover is **not observable**. At p = 4, the 2 × 2 run was not faster than
4 × 1 (12.92 vs 13.09 s nonblocking at N = 4096; slower at N = 1024). The halo carries at most
8 192 doubles = 64 KiB per step, against about 268 MB of grid traffic per step (16.8 M cells ×
16 B). Halo bytes are not our bottleneck, and 2-D's column halos cost more per value because they
are strided (`MPI_Type_vector`).

### Q2 — Communication–computation overlap

| N | dims | p | blocking (s) | nonblocking (s) | nbtest (s) | halo wait: blocking → nonblocking → nbtest (s) |
|---|---|---|---|---|---|---|
| 1024 | 2×1 | 2 | 0.924 | 0.836 (**−9.5%**) | 0.818 (−11.5%) | 0.089 → 0.063 → 0.041 |
| 4096 | 2×1 | 2 | 12.963 | 13.129 (+1.3%) | 13.037 (+0.6%) | 0.434 → 0.366 → 0.085 |
| 1024 | 4×1 | 4 | 2.055 | 1.224 (−40.4%) | 1.203 (−41.5%) | 1.586 → 0.674 → 0.601 |
| 4096 | 2×2 | 4 | 15.141 | 12.916 (−14.7%) | 13.617 (−10.1%) | 4.462 → 1.888 → 1.110 |

- **At p = 2, overlap helps a little at N = 1024 (−9.5%) and not at all at N = 4096.** At
  N = 4096 there is little to hide: communication is only 3% of the run (0.43 of 13 s), and the
  inner update is bandwidth-bound anyway.
- **Overlap does not happen by itself, which is why the `nbtest` variant exists.** Open MPI's
  shared-memory transport sends small messages eagerly, but a 4 096-value row (32 KiB) goes
  through a rendezvous handshake that only progresses inside MPI calls; there is no background
  progress thread. With plain `nonblocking`, the transfer therefore mostly happens inside
  `MPI_Waitall`, *after* the inner update. The wait was 0.366 s, barely below blocking's 0.434 s.
  When we call `MPI_Testall` every 16 rows during the inner update, the wait falls to
  **0.085 s (−77%)**. The transfer genuinely moves under the computation. Total time still does
  not improve, because the polling has its own cost and the computation it overlaps with is
  bandwidth-bound.
- **The large gains at p = 4 (−15 to −41%) are not classic overlap; they come from
  oversubscription.** Two ranks share each physical core. A rank blocked in `MPI_Sendrecv`
  busy-polls and takes CPU time from its partner on the same core, so the partner takes longer to
  reach its own send. With non-blocking calls, both ranks post their messages immediately and get
  on with useful work, so far less time is spent spinning.

### Q3 — Where efficiency drops below 70%, and why the model fails

The simple model T_p ≈ T_serial/p + T_comm (T_comm = measured halo + allreduce time):

| N | p | measured T_p | T_serial/p + T_comm | efficiency |
|---|---|---|---|---|
| 1024 | 2 | 0.836 | 0.525 + 0.063 = 0.588 | **0.63** |
| 4096 | 2 | 13.129 | 6.896 + 0.425 = 7.321 | **0.53** |
| 4096 | 4 | 12.916 | 3.448 + 1.977 = 5.425 | 0.27 |

Efficiency is **already below 70% at p = 2** for both grid sizes, and the simple model
underestimates T_p by 1.4–2.4×. The model's assumption that compute time divides by p is what
breaks. Communication is small. Compute time does not shrink, because all cores share one memory
bus. A bandwidth-aware model fits the measurements:

T_p ≈ max( T_serial/p , B·cells·T / BW(p) ) + T_comm, with B ≈ 24 B/cell and BW(p) from `membench`:

| N | p | bandwidth term (s) | + T_comm (s) | model (s) | measured (s) |
|---|---|---|---|---|---|
| 4096 | 2 | 8.38·10⁹ × 24 / 16.4·10⁹ = 12.26 | 0.43 | **12.69** | 13.13 |
| 4096 | 4 | 8.38·10⁹ × 24 / 16.6·10⁹ = 12.11 | 1.98 | 14.09 | 12.92 (overlap hides part of T_comm) |

The same bound explains **weak scaling**. Total memory traffic grows with p, but bandwidth grows
only from 13.4 to 16.4 GB/s, so weak efficiency can be at most BW(p)/(p·BW(1)) = 0.61 at p = 2
(measured 0.55) and 0.31 at p = 4 (measured 0.20; the rest is lost to oversubscription). On a
cluster, each node has its own memory bus, so aggregate bandwidth grows with the node count and
this code would scale. On one laptop it cannot. Reducing bytes per update (temporal blocking,
32-bit floats) would help; adding cores does not.

### Q4 — Cost of the global convergence check

| N | p | K = 1 (s) | K = 50 (s) | K=1 / K=50 | compute K=1 → K=50 (s) | allreduce K=1 → K=50 (s) |
|---|---|---|---|---|---|---|
| 1024 | serial | 2.029 | 1.344 | 1.51 | 2.029 → 1.344 | – |
| 1024 | 2 | 1.838 | 0.901 | 2.04 | 1.606 → 0.843 | 0.020 → 0.003 |
| 1024 | 4 | 4.676 | 2.905 | 1.61 | 1.329 → 0.740 | **1.831** → 0.039 |
| 4096 | serial | 34.268 | 17.116 | 2.00 | 34.268 → 17.116 | – |
| 4096 | 2 | 23.342 | 15.382 | 1.52 | 22.239 → 14.931 | 0.102 → 0.061 |
| 4096 | 4 | 26.491 | 14.776 | 1.79 | 21.002 → 11.923 | **2.773** → 0.095 |

Checking every step makes the run **1.5–2.0× slower**, for two separate reasons:

1. **Computing the metric is the larger cost, even in serial (2.0× at N = 4096).** The tracking
   loop adds `fabs`, a compare and a running maximum per cell. GCC's vectorization report
   (`-fopt-info-vec`) confirms that the plain update loop is vectorized (AVX2, 32-byte vectors) and
   the tracking loop is not. Without `-ffast-math`, GCC may not turn `if (d > md) md = d` into a
   vector max, because that would change NaN and signed-zero behaviour. The tracking loop therefore
   runs scalar and becomes compute-bound instead of bandwidth-bound. The serial result shows this cost
   has nothing to do with MPI.
2. **The reduction is a global synchronization point.** `MPI_Allreduce` costs only 0.02–0.10 s
   over 500 steps at p = 2, but **1.8–2.8 s at p = 4**. Every step all ranks must arrive before
   any can continue, so any delay to one rank (a descheduled rank on an oversubscribed core,
   Windows background work) stalls all of them. With K = 50 this happens 10 times instead of 500.

With K = 50, both costs are paid on 2% of the steps. The price is that convergence is detected up
to 49 steps late, which is negligible here, since a 500-step run needs only ten checks.

*References:* Message Passing Interface Forum, *MPI: A Message-Passing Interface Standard,
Version 4.0* (2021): Cartesian topologies, non-blocking point-to-point, MPI-IO. Open MPI
documentation, shared-memory BTL (eager vs rendezvous protocols). S. Williams, A. Waterman,
D. Patterson, "Roofline: an insightful visual performance model for multicore architectures,"
CACM 52(4), 2009.

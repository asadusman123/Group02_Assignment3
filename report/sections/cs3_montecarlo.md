# Case Study 3 — Monte Carlo Option Pricing (OpenMP)

## Problem and why it parallelizes

We price a European call option whose underlying follows geometric Brownian motion,
S_T = S0·exp((r − σ²/2)T + σ√T·Z), with Z ~ N(0,1). The price is e^(−rT)·E[max(S_T − K, 0)],
estimated by averaging the payoff over M independent samples. Parameters: S0 = 100, K = 100,
r = 0.05, σ = 0.20, T = 1 year, M = 10⁸. The Black–Scholes closed form for these parameters is
**10.450584**.

Every sample is independent. A worker needs only its own random numbers and two running sums
(Σpayoff, Σpayoff²), and the workers interact once, at the end, to add those sums together. The
problem is therefore embarrassingly parallel: work is O(M), communication is O(p).

## Model and decomposition

**Model: OpenMP.** CS1 and CS5 use MPI and CS2 and CS4 use OpenMP, so CS3 is free. We chose shared
memory because the single node has 2 cores and nothing needs to be distributed. OpenMP also makes
the "one shared generator" failure mode (critical-analysis Q1) easy to construct with
`omp critical`.

**Decomposition: static data decomposition** (figure). Thread t owns samples [tM/p, (t+1)M/p).
Every sample costs the same, so a static split is balanced. The measured per-thread imbalance
(slowest/mean thread time) was 1.001 at p = 2 and 1.013 at p = 4.

![CS3 decomposition](figures/cs3_decomposition.png)

## Key implementation decisions

- **Generator: xoshiro256\*\*** (Blackman & Vigna, 2018), 256-bit state seeded through splitmix64.
  Box–Muller turns two uniforms into two normals, and both are used. Uniforms are drawn from
  (0, 1], never 0, so `log(u)` is always finite.
- **Independent streams by jump-ahead.** Each thread seeds the generator with the same seed and
  calls `jump()` t times. `jump()` advances the state by 2^128 steps, so thread streams are
  guaranteed disjoint. Thread 0 does not jump, so p = 1 reproduces the serial program bit for bit.
- **Deterministic reduction.** We did not use `reduction(+:…)`, because OpenMP does not specify
  the order in which it combines partial results. Each thread writes its two sums into its own
  64-byte-padded slot, and the slots are added in thread order after the parallel region.
- **Shared kernel.** `serial.c`, `parallel.c` and `assoc.c` call the same `mc_block()` from
  `mc_common.h`, so any difference between versions comes from parallelization, not from
  different arithmetic.

## Correctness validation

| check | result |
|---|---|
| Serial price vs Black–Scholes, M = 10⁸, seed 42 | 10.451719 ± 0.001472, i.e. 0.77 SE from 10.450584 |
| Serial vs parallel, p = 1 | bit-identical (10.4517186923) |
| Same seed and same p, two runs (`make check`, p = 1, 2, 4) | bit-identical every time |
| Parallel, p = 2 / p = 4 | 10.451805 / 10.451660, i.e. 0.83 / 0.73 SE from Black–Scholes |

Tolerance: a parallel estimate is accepted if it lies within 3 standard errors of the
Black–Scholes price. Every run in this study satisfies that.

## Speedup and efficiency

Machine: Intel Core i5-7300U (2 physical cores, 4 logical, 15 W), Ubuntu 26.04 under WSL2,
GCC 15.2.0 `-O3 -march=native`, `OMP_PROC_BIND=close`. Laptop in its default power configuration
(turbo enabled, on mains power). Nine interleaved rounds (serial, p = 1, 2, 4, repeated), with 5 s
of cool-down between runs.

| version | p | median (s) | min (s) | max (s) | spread max/min | S(p) median | S(p) min | E(p) min |
|---|---|---|---|---|---|---|---|---|
| serial | 1 | 10.220 | 4.504 | 17.153 | 3.81 | 1.00 | 1.00 | 1.00 |
| parallel | 1 | 6.272 | 4.634 | 11.508 | 2.48 | 1.63 | 0.97 | 0.97 |
| parallel | 2 | 4.758 | 3.211 | 8.204 | 2.55 | 2.15 | 1.40 | 0.70 |
| parallel | 4 (HT) | 3.738 | 2.565 | 5.334 | 2.08 | 2.73 | 1.76 | 0.44 |

![CS3 speedup](../cs3_montecarlo/results/speedup.png)

**Why the minimum is the primary statistic here.** Identical work varied by up to 3.8× between runs
(serial: 4.5 s to 17.2 s). With that much noise, the median of 9 runs depends on which runs
happened to land in a slow phase. The clearest evidence is that serial and parallel p = 1 execute
the same code on the same samples: their **minimums agree within 3% (4.50 vs 4.63 s)**, but their
**medians differ by 1.63× (10.2 vs 6.3 s)**. A median-based "speedup" of 2.15 on 2 cores is
therefore an artefact of noise, not a result. We report the median as the brief requires, but we
interpret the minimum, which is the run with the least interference from throttling and the OS.

## Critical analysis

### Q1 — Why RNG matters

We ran two broken strategies at p = 4 (`./parallel M 42 shared|sameseed`):

| strategy | M | throughput (M samples/s) | price | reported SE | true SE |
|---|---|---|---|---|---|
| independent streams (jump) | 10⁸ | 15.68 | 10.451660 | 0.001472 | 0.001472 |
| one shared generator in `omp critical` | 2·10⁷ | **3.81** | 10.452101 | 0.003292 | 0.003292 |
| every thread uses the same seed | 10⁸ | 13.73 | 10.452449 | 0.001472 | **0.002945** |

- **Shared generator → serialization.** The price is still statistically correct (0.46 SE from
  Black–Scholes), but throughput collapses to 3.81 M samples/s. That is 4.1× slower than the
  correct 4-thread version, and 5.8× slower than the *serial* program's best of 22.2 M/s. Every
  pair of normals needs the lock, so only one thread can draw at a time. On top of that, the
  32-byte generator state and the lock bounce between the two cores' caches on every draw, so
  four threads end up slower than one.
- **Same seed → correlated streams and a wrong error bar.** All four threads replay the same
  sequence, so the 10⁸ "samples" are 4 copies of 2.5·10⁷. We verified this directly: the p = 4
  `sameseed` price, **10.4524486110**, is bit-identical to the serial program with M = 2.5·10⁷
  (`./serial 25000000 42`), whose SE is 0.0029447. The program reports SE = 0.001472 because it
  assumes 10⁸ independent samples, so the **error bar is too small by a factor √p = 2**. A
  95% confidence interval built from it would cover the true price far less often than 95% of the
  time. Nothing in the output warns you, and that is what makes this bug dangerous.

### Q2 — Statistical vs parallel efficiency

The standard error is SE = e^(−rT)·σ_payoff/√M, with e^(−rT)·σ_payoff = 14.72 for our parameters.
With a fixed time budget t, p threads process M = S(p)·M₁ samples, so

    SE_p / SE_1 = 1/√S(p).

**Fixed-time experiment.** We chose M for each p so that every configuration would finish in the
serial best time of 4.5 s, using the min-based throughput from the strong-scaling runs
(M = 10⁸ × 4.504 / T_min(p)). Each configuration was run 3 times, interleaved
(`results/fixed_time_min.csv`). The SE depends only on M and the seed, so it is exact and
identical across rounds. The wall times are not:

| p | M (samples) | SE | SE₁/SE_p | √(M_p/M₁) | best time (s) | all 3 times (s) |
|---|---|---|---|---|---|---|
| 1 | 97,190,000 | 0.0014934 | 1.00 | 1.00 | 6.01 | 6.01, 14.66, 7.30 |
| 2 | 140,270,000 | 0.0012430 | 1.20 | 1.20 | 4.61 | 5.46, 4.61, 13.38 |
| 4 | 175,590,000 | 0.0011109 | 1.34 | 1.34 | 7.61 | 9.97, 7.61, 9.97 |

With the same time budget, 4 threads reduce the error by only **1.34×**, which is √1.81: the
square root of the throughput gain. They do not reduce it by 1.81×, and certainly not by 4×. A
parallel speedup S buys a √S reduction in error. Halving the error needs 4× as many samples, so it
would need S = 4 at fixed time, more than this machine can deliver even in its best state. In
accuracy terms, "speedup" means either reaching the same SE in 1/S of the time, or reaching a
√S-times smaller SE in the same time. The usual Monte Carlo figure of merit, 1/(SE²·t), grows
linearly with S, and that is what parallelism actually improves.

During this batch the laptop was in a slower state than in the strong-scaling runs: no
configuration reached the 4.5 s target, and p = 1 ranged from 6.0 to 14.7 s for identical work.
The table therefore shows the error side of the trade-off exactly, while its time side is subject
to the same thermal and turbo noise discussed in Q3.

### Q3 — Why it (should) scale almost perfectly

**Serial fraction of the code.** The only non-parallel work is thread start-up, p generator
seedings and at most p − 1 jumps (each jump is 256 generator steps), and the final p-term
reduction. That is a few microseconds against a 4.5 s run: f_code < 10⁻⁵. By Amdahl's law the
code alone would give S(2) ≈ 2.00.

**Measured serial fraction.** Karp–Flatt, f = (1/S − 1/p)/(1 − 1/p), gives f ≈ 0.43 at both p = 2
and p = 4 (min-based). This f is not serial code. It is hardware losses that Amdahl's model does
not include, because Amdahl assumes every core runs at the same speed regardless of how many are
busy:

1. **Turbo and power limit.** The i5-7300U turbos to its highest clock with one core active, and
   both its turbo table and its 15 W package power limit lower the clock when both cores are
   busy. A one-thread run therefore gets a faster core than each thread of a two-thread run. The
   3.8× run-to-run spread for identical serial work shows how strongly clock and thermal state
   drive time on this machine.
2. **Hyper-threading.** Going from p = 2 to p = 4 improved time by 1.25× (3.211 → 2.565 s), not
   2×. Two hardware threads share one core's floating-point units. The gain comes from one thread
   filling the stall cycles left by the long-latency `exp`/`log`/`sin`/`cos` dependency chains of
   the other.
3. **OS and virtualization interference.** WSL2 runs as a Hyper-V VM. With every core busy, any
   Windows background activity pre-empts one of our threads, and the parallel region waits for
   the slowest thread.
4. **Reduction and start-up.** Negligible, see above. Serial vs p = 1 differs by 3% (4.50 vs
   4.63 s), which bounds the cost of the OpenMP runtime plus noise.

### Q4 — Reproducibility

- **Same seed, same p → bit-identical** (`make check`, rerun twice for p = 1, 2, 4). This holds
  because the stream assignment and the combining order of the partial sums are both fixed.
- **Changing p changes the result:** p = 1: 10.4517186923, p = 2: 10.4518049874,
  p = 4: 10.4516596552. The differences, about 1·10⁻⁴, are about 0.1 SE. There are two possible
  causes, and we measured them separately:
  1. **Different stream assignment (dominant).** With p threads, sample i comes from stream ⌊ip/M⌋,
     so a different p uses a different set of random numbers. The differences are of statistical
     size, 0.1 SE.
  2. **Floating-point non-associativity (negligible here).** `./assoc` draws the *same* 10⁸
     samples once and adds them in different groupings: one accumulator, or 2, 4 or 8 chunk
     accumulators combined in order. The prices differ by only **1.3·10⁻¹² to 2.1·10⁻¹²**, which is
     about 10⁻⁹ SE and 10⁸ times smaller than effect 1. It is still not zero, which is why we
     avoid `reduction(+:…)`: its unspecified combining order could change the last digits from
     run to run even at fixed p.

*References:* D. Blackman, S. Vigna, "Scrambled linear pseudorandom number generators," ACM
TOMS 47(4), 2021 (xoshiro256\*\*, jump function). F. Black, M. Scholes, "The pricing of options
and corporate liabilities," J. Political Economy 81(3), 1973. G. E. P. Box, M. E. Muller, "A note
on the generation of random normal deviates," Ann. Math. Stat. 29(2), 1958. A. H. Karp,
H. P. Flatt, "Measuring parallel processor performance," CACM 33(5), 1990.

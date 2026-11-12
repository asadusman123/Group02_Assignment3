# CS3006 Parallel and Distributed Computing — Assignment 3
## Parallel Case Studies with OpenMP and MPI

**Group 02** · Asad Usman (23L-0740), BSCS-7H

*Report skeleton (Day-7 checkpoint draft). Sections marked TODO are in progress.*

## 1. Introduction

### Team and work split
This group has one member, who is working solo (arrangement with the instructor: *TODO: add once
confirmed*). All five case studies were designed, implemented, measured and written up by
Asad Usman. AI-tool usage, as permitted by the TA, is disclosed in Section 8.

### Machine

| item | value |
|---|---|
| CPU | Intel Core i5-7300U @ 2.60 GHz (Kaby Lake, 15 W TDP, turbo enabled) |
| cores | 2 physical, 4 logical (2-way hyper-threading); logical CPUs 0/1 share core 0, 2/3 share core 1 |
| caches | L1d 32 KiB and L2 256 KiB per core; L3 3 MiB shared |
| memory | 9.7 GiB visible to the Linux VM |
| OS | Ubuntu 26.04.1 LTS under WSL2 (Hyper-V virtual machine on Windows) |
| compiler | GCC 15.2.0, flags `-O3 -march=native -std=c11` for every serial and parallel version |
| MPI | Open MPI 5.0.10 (Debian/Ubuntu package), shared-memory transport within the node |
| plotting | Python 3 + matplotlib |

Because the machine has only 2 physical cores, our scaling runs use p ∈ {1, 2, 4}. p = 4 uses
hyper-threading for OpenMP, and oversubscribes the cores (`--map-by :OVERSUBSCRIBE`) for MPI.
We mark p = 4 as such in every table and graph, and we do not treat a speedup above 2 as
meaningful.

### How timings were taken
- Only the parallel region is timed: OpenMP with `omp_get_wtime`/`clock_gettime`, MPI with
  `MPI_Wtime` between two `MPI_Barrier` calls, reporting the maximum over ranks. Input generation
  and file I/O are excluded, and their times are printed separately.
- Every configuration is run several times (3–9 rounds, stated per table). Runs are **interleaved**
  (serial, p = 1, p = 2, p = 4, serial, …) with a short cool-down between them, so slow drift in
  clock speed affects all configurations equally.
- We report the **median** as required. Because this laptop's clock varies strongly with
  temperature and turbo state, we also report the **minimum** and the **max/min spread**; where
  the two disagree, we say which one we trust and why.
- Thread placement: `OMP_PLACES=cores OMP_PROC_BIND=close` (OpenMP) and `--bind-to core` (MPI,
  p ≤ 2). Inside WSL2 this pins work to virtual CPUs only; see CS3, Q3, for its consequences.
- Every benchmark is reproducible from the repository: `./bench.sh && python3 analyze.py` in each
  case-study directory regenerates the raw log, the tables and the graphs in `results/`.

## 2. Case Study 1 — Heat Diffusion (MPI)
*TODO: insert `cs1_heat_mpi.md` once the benchmarks are complete.*

## 3. Case Study 2 — k-mer Counting (OpenMP)
*See `cs2_kmer.md` (complete).*

## 4. Case Study 3 — Monte Carlo Option Pricing (OpenMP)
*See `cs3_montecarlo.md` (complete).*

## 5. Case Study 4 — PageRank (OpenMP)
*TODO.*

## 6. Case Study 5 — Distributed Sample Sort (MPI)
*TODO.*

## 7. Cross-case comparison
*TODO: one table with the model, the scaling achieved and the dominant bottleneck for each case
study, plus which scaled best and why.*

| case | model | best speedup (p) | dominant bottleneck |
|---|---|---|---|
| CS1 heat | MPI | TODO | TODO |
| CS2 k-mer | OpenMP | 1.70 (4), k = 8 private | cache capacity (k = 8); memory latency (k = 12) |
| CS3 Monte Carlo | OpenMP | 1.70 (4) | clock frequency (turbo/power) and the hyper-threading limit |
| CS4 PageRank | OpenMP | TODO | TODO |
| CS5 sample sort | MPI | TODO | TODO |

## 8. Individual contributions and use of AI tools
*TODO: summary of CONTRIBUTIONS.md. AI assistance (Claude) was used for code drafting and
analysis, with the TA's permission; the author verified, ran and can explain all code and
results.*

## 9. References
*TODO: merged from the per-section reference lists.*

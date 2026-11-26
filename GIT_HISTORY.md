# Git history — Group02_Assignment3

- Repository: https://github.com/asadusman123/Group02_Assignment3
- Generated: 2026-11-26 21:45 from `git log`
- Commits: 11, from 2026-11-09 to 2026-11-25

## All commits (oldest first)

| # | date | commit | author | message | files | +lines | −lines |
|---|---|---|---|---|---|---|---|
| 1 | 2026-11-09 17:04 | `49a5956` | Asad Usman | Initial repo structure | 8 | 9 | 0 |
| 2 | 2026-11-12 15:19 | `ca154b8` | Asad Usman | CS3: serial Monte Carlo pricer with xoshiro256** and Black-Scholes check | 6 | 228 | 0 |
| 3 | 2026-11-12 15:54 | `e5976de` | Asad Usman | CS3: benchmark script, analysis, results | 17 | 451 | 4 |
| 4 | 2026-11-12 16:06 | `1bc49c0` | Asad Usman | CS3: 9-round benchmark with min- and median-based speedup | 18 | 80 | 69 |
| 5 | 2026-11-12 16:37 | `ca37261` | Asad Usman | CS2: k-mer counting (A/B/C), boundary and false-sharing demos, benchmarks; CS3 Q2 | 20 | 1455 | 3 |
| 6 | 2026-11-12 16:47 | `ef474d5` | Asad Usman | CS3: rerun with one thread per physical core | 9 | 117 | 79 |
| 7 | 2026-11-12 18:35 | `5a820bb` | Asad Usman | CS1: benchmark results and report section | 28 | 2415 | 78 |
| 8 | 2026-11-12 18:59 | `8059fc4` | Asad Usman | CS4: PageRank pull/push, schedule sweep, benchmarks | 10 | 2039 | 0 |
| 9 | 2026-11-25 20:36 | `69a9d7a` | Asad Usman | CS5: MPI sample sort results; CS4 report section | 18 | 1153 | 1 |
| 10 | 2026-11-25 20:40 | `dfe6611` | Asad Usman | CS5 report section | 2 | 197 | 0 |
| 11 | 2026-11-25 21:11 | `c607c25` | Asad Usman | Final report, CONTRIBUTIONS, graph labels | 28 | 906 | 6 |

## Files changed per commit

### `49a5956` Initial repo structure

2026-11-09 17:04 · Asad Usman

```text
 .gitignore                    | 4 ++++
 cs1_heat_mpi/README.md        | 1 +
 cs2_kmer_openmp/README.md     | 1 +
 cs3_montecarlo/README.md      | 1 +
 cs4_pagerank_openmp/README.md | 1 +
 cs5_samplesort_mpi/README.md  | 1 +
 report/.gitkeep               | 0
 slides/.gitkeep               | 0
 8 files changed, 9 insertions(+)
```

### `ca154b8` CS3: serial Monte Carlo pricer with xoshiro256** and Black-Scholes check

2026-11-12 15:19 · Asad Usman

```text
 cs3_montecarlo/Makefile                    |  14 +++++
 cs3_montecarlo/Makefile:Zone.Identifier    |   2 +
 cs3_montecarlo/mc_common.h                 | 158 +++++++++++++++++++++++++++++++++++++++++++++++++
 cs3_montecarlo/mc_common.h:Zone.Identifier |   2 +
 cs3_montecarlo/serial.c                    |  50 ++++++++++++++++
 cs3_montecarlo/serial.c:Zone.Identifier    |   2 +
 6 files changed, 228 insertions(+)
```

### `e5976de` CS3: benchmark script, analysis, results

2026-11-12 15:54 · Asad Usman

```text
 cs3_montecarlo/Makefile                   |  19 +++++--
 cs3_montecarlo/README.md                  |  31 +++++++++++-
 cs3_montecarlo/README.md:Zone.Identifier  |   2 +
 cs3_montecarlo/analyze.py                 | 115 ++++++++++++++++++++++++++++++++++++++++++
 cs3_montecarlo/analyze.py:Zone.Identifier |   2 +
 cs3_montecarlo/bench.sh                   |  76 ++++++++++++++++++++++++++++
 cs3_montecarlo/bench.sh:Zone.Identifier   |   2 +
 cs3_montecarlo/parallel.c                 | 135 ++++++++++++++++++++++++++++++++++++++++++++++++++
 cs3_montecarlo/parallel.c:Zone.Identifier |   2 +
 cs3_montecarlo/results/fixed_time.csv     |   4 ++
 cs3_montecarlo/results/machine.txt        |   9 ++++
 cs3_montecarlo/results/rng_modes.csv      |   4 ++
 cs3_montecarlo/results/speedup.pdf        | Bin 0 -> 17383 bytes
 cs3_montecarlo/results/speedup.png        | Bin 0 -> 118192 bytes
 cs3_montecarlo/results/strong.csv         |  21 ++++++++
 cs3_montecarlo/results/tables.md          |  33 ++++++++++++
 cs3_montecarlo/serial_omp                 | Bin 0 -> 16696 bytes
 17 files changed, 451 insertions(+), 4 deletions(-)
```

### `1bc49c0` CS3: 9-round benchmark with min- and median-based speedup

2026-11-12 16:06 · Asad Usman

```text
 .gitignore                                 |   2 ++
 cs3_montecarlo/Makefile:Zone.Identifier    |   2 --
 cs3_montecarlo/README.md:Zone.Identifier   |   2 --
 cs3_montecarlo/analyze.py                  |  25 ++++++++++++++--------
 cs3_montecarlo/analyze.py:Zone.Identifier  |   2 --
 cs3_montecarlo/bench.sh                    |   4 ++--
 cs3_montecarlo/bench.sh:Zone.Identifier    |   2 --
 cs3_montecarlo/mc_common.h:Zone.Identifier |   2 --
 cs3_montecarlo/parallel.c:Zone.Identifier  |   2 --
 cs3_montecarlo/results/fixed_time.csv      |   6 +++---
 cs3_montecarlo/results/machine.txt         |   6 +++---
 cs3_montecarlo/results/rng_modes.csv       |   6 +++---
 cs3_montecarlo/results/speedup.pdf         | Bin 17383 -> 19199 bytes
 cs3_montecarlo/results/speedup.png         | Bin 118192 -> 149625 bytes
 cs3_montecarlo/results/strong.csv          |  56 +++++++++++++++++++++++++++++++------------------
 cs3_montecarlo/results/tables.md           |  30 +++++++++++++-------------
 cs3_montecarlo/serial.c:Zone.Identifier    |   2 --
 cs3_montecarlo/serial_omp                  | Bin 16696 -> 0 bytes
 18 files changed, 80 insertions(+), 69 deletions(-)
```

### `ca37261` CS2: k-mer counting (A/B/C), boundary and false-sharing demos, benchmarks; CS3 Q2

2026-11-12 16:37 · Asad Usman

```text
 .gitignore                                |   2 +
 cs2_kmer_openmp/Makefile                  |  34 ++++++
 cs2_kmer_openmp/README.md                 |  46 +++++++-
 cs2_kmer_openmp/analyze.py                | 160 ++++++++++++++++++++++++++++
 cs2_kmer_openmp/bench.sh                  |  70 +++++++++++++
 cs2_kmer_openmp/kmer_common.h             | 120 +++++++++++++++++++++
 cs2_kmer_openmp/membench.c                |  91 ++++++++++++++++
 cs2_kmer_openmp/parallel.c                | 197 +++++++++++++++++++++++++++++++++++
 cs2_kmer_openmp/results/machine.txt       |  18 ++++
 cs2_kmer_openmp/results/raw.log           | 283 ++++++++++++++++++++++++++++++++++++++++++++++++++
 cs2_kmer_openmp/results/speedup.pdf       | Bin 0 -> 20628 bytes
 cs2_kmer_openmp/results/speedup.png       | Bin 0 -> 143251 bytes
 cs2_kmer_openmp/results/tables.md         | 115 ++++++++++++++++++++
 cs2_kmer_openmp/serial.c                  |  55 ++++++++++
 cs3_montecarlo/Makefile                   |   7 +-
 cs3_montecarlo/README.md                  |   2 +
 cs3_montecarlo/assoc.c                    |  63 +++++++++++
 cs3_montecarlo/results/fixed_time_min.csv |  10 ++
 report/figures/cs3_decomposition.png      | Bin 0 -> 122576 bytes
 report/sections/cs3_montecarlo.md         | 185 +++++++++++++++++++++++++++++++++
 20 files changed, 1455 insertions(+), 3 deletions(-)
```

### `ef474d5` CS3: rerun with one thread per physical core

2026-11-12 16:47 · Asad Usman

```text
 cs3_montecarlo/analyze.py             |  23 ++++++++++++-----
 cs3_montecarlo/bench.sh               |  32 +++++++++++++++---------
 cs3_montecarlo/results/fixed_time.csv |  14 ++++++++---
 cs3_montecarlo/results/machine.txt    |  13 +++++++---
 cs3_montecarlo/results/rng_modes.csv  |   6 ++---
 cs3_montecarlo/results/speedup.pdf    | Bin 19199 -> 19183 bytes
 cs3_montecarlo/results/speedup.png    | Bin 149625 -> 140028 bytes
 cs3_montecarlo/results/strong.csv     |  72 +++++++++++++++++++++++++++---------------------------
 cs3_montecarlo/results/tables.md      |  36 +++++++++++++++------------
 9 files changed, 117 insertions(+), 79 deletions(-)
```

### `5a820bb` CS1: benchmark results and report section

2026-11-12 18:35 · Asad Usman

```text
 .gitignore                           |   2 +
 cs1_heat_mpi/Makefile                |  30 +++++++
 cs1_heat_mpi/README.md               |  37 +++++++-
 cs1_heat_mpi/analyze.py              | 228 ++++++++++++++++++++++++++++++++++++++++++++++
 cs1_heat_mpi/bench.sh                |  89 ++++++++++++++++++
 cs1_heat_mpi/heat_common.h           |  56 ++++++++++++
 cs1_heat_mpi/parallel.c              | 268 +++++++++++++++++++++++++++++++++++++++++++++++++++++++
 cs1_heat_mpi/results/machine.txt     |  20 +++++
 cs1_heat_mpi/results/phases.png      | Bin 0 -> 66944 bytes
 cs1_heat_mpi/results/raw.log         | 176 ++++++++++++++++++++++++++++++++++++
 cs1_heat_mpi/results/scaling.pdf     | Bin 0 -> 23579 bytes
 cs1_heat_mpi/results/scaling.png     | Bin 0 -> 206361 bytes
 cs1_heat_mpi/results/tables.md       | 104 +++++++++++++++++++++
 cs1_heat_mpi/serial.c                |  76 ++++++++++++++++
 cs4_pagerank_openmp/Makefile         |  29 ++++++
 cs4_pagerank_openmp/README.md        |  35 +++++++-
 cs4_pagerank_openmp/analyze.py       | 144 ++++++++++++++++++++++++++++++
 cs4_pagerank_openmp/bench.sh         |  55 ++++++++++++
 cs4_pagerank_openmp/gen.c            | 139 +++++++++++++++++++++++++++++
 cs4_pagerank_openmp/parallel.c       | 171 +++++++++++++++++++++++++++++++++++
 cs4_pagerank_openmp/pr_common.h      | 110 +++++++++++++++++++++++
 cs4_pagerank_openmp/serial.c         |  68 ++++++++++++++
 report/figures/cs1_decomposition.png | Bin 0 -> 84821 bytes
 report/figures/cs2_decomposition.png | Bin 0 -> 129681 bytes
 report/sections/00_introduction.md   |  81 +++++++++++++++++
 report/sections/cs1_heat_mpi.md      | 223 +++++++++++++++++++++++++++++++++++++++++++++
 report/sections/cs2_kmer.md          | 201 +++++++++++++++++++++++++++++++++++++++++
 report/sections/cs3_montecarlo.md    | 151 ++++++++++++++++---------------
 28 files changed, 2415 insertions(+), 78 deletions(-)
```

### `8059fc4` CS4: PageRank pull/push, schedule sweep, benchmarks

2026-11-12 18:59 · Asad Usman

```text
 cs4_pagerank_openmp/degrees_s20.csv        | 854 +++++++++++++++++++++++++++++++++++++++++++++++++
 cs4_pagerank_openmp/results/degrees.csv    | 854 +++++++++++++++++++++++++++++++++++++++++++++++++
 cs4_pagerank_openmp/results/degrees.png    | Bin 0 -> 101909 bytes
 cs4_pagerank_openmp/results/graph_info.txt |   6 +
 cs4_pagerank_openmp/results/machine.txt    |  11 +
 cs4_pagerank_openmp/results/raw.log        | 245 ++++++++++++++
 cs4_pagerank_openmp/results/schedules.png  | Bin 0 -> 113457 bytes
 cs4_pagerank_openmp/results/speedup.pdf    | Bin 0 -> 16519 bytes
 cs4_pagerank_openmp/results/speedup.png    | Bin 0 -> 107508 bytes
 cs4_pagerank_openmp/results/tables.md      |  69 ++++
 10 files changed, 2039 insertions(+)
```

### `69a9d7a` CS5: MPI sample sort results; CS4 report section

2026-11-25 20:36 · Asad Usman

```text
 .gitignore                                  |   1 +
 cs5_samplesort_mpi/Makefile                 |  30 +++++++
 cs5_samplesort_mpi/README.md                |  38 +++++++-
 cs5_samplesort_mpi/analyze.py               | 167 +++++++++++++++++++++++++++++++++++
 cs5_samplesort_mpi/bench.sh                 |  47 ++++++++++
 cs5_samplesort_mpi/parallel.c               | 228 ++++++++++++++++++++++++++++++++++++++++++++++++
 cs5_samplesort_mpi/pingpong.c               |  55 ++++++++++++
 cs5_samplesort_mpi/results/machine.txt      |  12 +++
 cs5_samplesort_mpi/results/oversampling.png | Bin 0 -> 87091 bytes
 cs5_samplesort_mpi/results/phases.pdf       | Bin 0 -> 18283 bytes
 cs5_samplesort_mpi/results/phases.png       | Bin 0 -> 81153 bytes
 cs5_samplesort_mpi/results/pingpong.png     | Bin 0 -> 92129 bytes
 cs5_samplesort_mpi/results/raw.log          | 130 +++++++++++++++++++++++++++
 cs5_samplesort_mpi/results/tables.md        | 103 ++++++++++++++++++++++
 cs5_samplesort_mpi/serial.c                 |  45 ++++++++++
 cs5_samplesort_mpi/ss_common.h              |  93 ++++++++++++++++++++
 report/figures/cs4_decomposition.png        | Bin 0 -> 206780 bytes
 report/sections/cs4_pagerank.md             | 205 +++++++++++++++++++++++++++++++++++++++++++
 18 files changed, 1153 insertions(+), 1 deletion(-)
```

### `dfe6611` CS5 report section

2026-11-25 20:40 · Asad Usman

```text
 report/figures/cs5_decomposition.png | Bin 0 -> 129069 bytes
 report/sections/cs5_samplesort.md    | 197 +++++++++++++++++++++++++++++++++++++++++++++++++++++++
 2 files changed, 197 insertions(+)
```

### `c607c25` Final report, CONTRIBUTIONS, graph labels

2026-11-25 21:11 · Asad Usman

```text
 CONTRIBUTIONS.md                            |  66 +++++++
 cs4_pagerank_openmp/analyze.py              |   4 +-
 cs4_pagerank_openmp/results/degrees.png     | Bin 101909 -> 121192 bytes
 cs4_pagerank_openmp/results/schedules.png   | Bin 113457 -> 125740 bytes
 cs5_samplesort_mpi/analyze.py               |   8 +-
 cs5_samplesort_mpi/results/oversampling.png | Bin 87091 -> 100527 bytes
 cs5_samplesort_mpi/results/phases.png       | Bin 81153 -> 120220 bytes
 cs5_samplesort_mpi/results/pingpong.png     | Bin 92129 -> 110964 bytes
 report/Group02_report.pdf                   | Bin 0 -> 2943829 bytes
 report/appendix.md                          | 453 ++++++++++++++++++++++++++++++++++++++++++++++++
 report/build.sh                             |   9 +
 report/fig/cs1_decomposition.png            | Bin 0 -> 84821 bytes
 report/fig/cs1_phases.png                   | Bin 0 -> 66944 bytes
 report/fig/cs1_scaling.png                  | Bin 0 -> 206361 bytes
 report/fig/cs2_decomposition.png            | Bin 0 -> 129681 bytes
 report/fig/cs2_speedup.png                  | Bin 0 -> 143251 bytes
 report/fig/cs3_decomposition.png            | Bin 0 -> 122576 bytes
 report/fig/cs3_speedup.png                  | Bin 0 -> 140028 bytes
 report/fig/cs4_decomposition.png            | Bin 0 -> 206780 bytes
 report/fig/cs4_degrees.png                  | Bin 0 -> 121192 bytes
 report/fig/cs4_schedules.png                | Bin 0 -> 125740 bytes
 report/fig/cs4_speedup.png                  | Bin 0 -> 121687 bytes
 report/fig/cs5_decomposition.png            | Bin 0 -> 129069 bytes
 report/fig/cs5_oversampling.png             | Bin 0 -> 100527 bytes
 report/fig/cs5_phases.png                   | Bin 0 -> 120220 bytes
 report/fig/cs5_pingpong.png                 | Bin 0 -> 110964 bytes
 report/report.md                            | 348 +++++++++++++++++++++++++++++++++++++
 report/style.css                            |  24 +++
 28 files changed, 906 insertions(+), 6 deletions(-)
```

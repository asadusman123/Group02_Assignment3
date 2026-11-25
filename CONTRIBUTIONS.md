# CONTRIBUTIONS — Group 02

> **Before submitting:** check the hours below and adjust them to your real figures; add the
> instructor's solo-work approval (date and wording) where marked.

## Team

| member | roll no. | role |
|---|---|---|
| Asad Usman | 23L-0740 | sole member: lead **and** reviewer of all five case studies |

Group 02 has a single member. *(Instructor approval to work individually: add date and wording
here.)* The brief's lead/reviewer split between two members (Section 7) therefore does not apply.
Every case study was led, built, rerun and checked by the same person.

## Ownership per case study

| case study | lead | reviewer | directory |
|---|---|---|---|
| CS1 Heat diffusion (MPI) | Asad Usman | Asad Usman | `cs1_heat_mpi/` |
| CS2 k-mer counting (OpenMP) | Asad Usman | Asad Usman | `cs2_kmer_openmp/` |
| CS3 Monte Carlo pricing (OpenMP) | Asad Usman | Asad Usman | `cs3_montecarlo/` |
| CS4 PageRank (OpenMP) | Asad Usman | Asad Usman | `cs4_pagerank_openmp/` |
| CS5 Sample sort (MPI) | Asad Usman | Asad Usman | `cs5_samplesort_mpi/` |
| Report, slides, integration | Asad Usman | — | `report/`, `slides/` |

## Tasks and approximate hours

| task | approx. hours |
|---|---|
| Environment: Ubuntu 26.04 on WSL2, GCC 15, Open MPI 5 (incl. the Ubuntu 26.04 `mpirun` help-file bug and oversubscription flags), private GitHub repo | 2 |
| CS3: serial + OpenMP versions, RNG/jump-ahead design, `assoc` test, three benchmark runs (incl. the rerun after discovering the WSL2 core layout) | 4 |
| CS2: versions A/B/C/N, false-sharing demo, `membench` memory probe, benchmark | 4 |
| CS1: serial + MPI (1-D/2-D, blocking/non-blocking/`nbtest`), MPI-IO verification, strong/weak scaling, K study | 5 |
| CS4: R-MAT generator, pull/push/racy push, schedule sweep, benchmark | 4 |
| CS5: sample sort with tie-breaking splitters, ping-pong, oversampling study, benchmark | 4 |
| Analysis of results and critical-analysis write-ups, report assembly | 5 |
| Slides and presentation preparation | 2 |
| **total** | **≈ 30** |

Benchmark wall-clock time (unattended runs on the laptop) is not included: about 2.5 hours in total.

## Commits

All commits are by Asad Usman (`asadusman123`). Main milestones (`git log --oneline` for the full history):

| commit message (abridged) | content |
|---|---|
| Initial repo structure | directory layout, READMEs |
| CS3: serial Monte Carlo pricer / OpenMP version / benchmarks / rerun with one thread per physical core | `cs3_montecarlo/*` |
| CS2: k-mer counting (A/B/C), boundary and false-sharing demos, benchmarks; CS3 Q2 | `cs2_kmer_openmp/*` |
| CS1: MPI heat diffusion …; benchmark results and report section | `cs1_heat_mpi/*` |
| CS4: PageRank pull/push, schedule sweep, benchmarks | `cs4_pagerank_openmp/*` |
| CS5: MPI sample sort results; CS4/CS5 report sections | `cs5_samplesort_mpi/*`, `report/sections/*` |
| Final report, slides, CONTRIBUTIONS | `report/`, `slides/`, this file |

## Use of AI tools

With the TA's permission, an AI assistant (Claude, Anthropic) was used to draft the code, the
benchmark/analysis scripts and the report text, and to help interpret the measurements. All builds,
correctness checks and benchmark runs were carried out by Asad Usman on his own machine, and every
number in the report comes from those runs (raw logs in each `results/` directory).

## Peer evaluation

Not applicable (single-member group).

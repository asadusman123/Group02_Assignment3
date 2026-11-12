# Case Study 3 - Monte Carlo European Call Pricing (OpenMP)

## Build
    make            # builds ./serial and ./parallel  (gcc -O3 -march=native, -fopenmp for parallel)
    make check      # p=1 must equal serial exactly; reruns with the same p must agree exactly

## Run
    ./serial [M] [seed]
    OMP_NUM_THREADS=p OMP_PROC_BIND=close ./parallel [M] [seed] [mode]

Defaults: `M = 100000000`, `seed = 42`, `mode = jump`.

| mode       | meaning                                                              |
|------------|----------------------------------------------------------------------|
| `jump`     | correct version: per-thread xoshiro256** stream via `jump()` (2^128 apart) |
| `shared`   | demo: one generator shared by all threads inside `omp critical`      |
| `sameseed` | demo: every thread seeds its own generator with the same seed        |

## Parameters
S0 = 100, K = 100, r = 0.05, sigma = 0.20, T = 1 year.
Black-Scholes reference price: 10.450584.

## Output
Human-readable lines plus one `RESULT ...` line for scripts. Only the sampling
region is timed (generator seeding and jumps are inside it and included).

## Files
- `mc_common.h` - RNG (xoshiro256**, splitmix64 seeding), Box-Muller, sampling kernel, Black-Scholes
- `serial.c`    - serial baseline
- `parallel.c`  - OpenMP version
- `assoc.c`     - isolates floating-point non-associativity: same samples, different summation grouping (`./assoc [M] [seed]`)
- `bench.sh`, `analyze.py` - timing runs and tables/graphs (`./bench.sh && python3 analyze.py`)

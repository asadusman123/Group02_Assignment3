## Graph

R-MAT scale 20 (a,b,c,d = 0.57,0.19,0.19,0.05), seed 2026: n = 1,048,576 vertices, m = 16,085,679 edges (average degree 15.34), max in-degree 39,615, max out-degree 39,581, dangling (out-degree 0) 501,933 (47.9%), isolated 401,900.

## Verification against serial ranks (tolerance: L1 distance < 1e-9)

| version | p | schedule | iterations | L1 distance to serial | result |
|---|---|---|---|---|---|
| pull | 1 | static | 24 | 0.000e+00 | PASS |
| pull | 2 | static | 24 | 5.689e-13 | PASS |
| pull | 4 | static | 24 | 9.065e-13 | PASS |
| push | 1 | static | 24 | 0.000e+00 | PASS |
| push | 2 | static | 24 | 5.688e-13 | PASS |
| push | 4 | static | 24 | 9.064e-13 | PASS |
| pushna | 1 | static | 24 | 0.000e+00 | PASS |
| pushna | 2 | static | 200 | 2.174e-03 | FAIL |
| pushna | 4 | static | 200 | 3.106e-03 | FAIL |

## Strong scaling (median of 5 runs; serial 2.713 s for 24 iterations)

| version | schedule | p | median (s) | min (s) | S median | E median | S min | M edges/s | imbalance R | edges max/mean |
|---|---|---|---|---|---|---|---|---|---|---|
| serial | - | 1 | 2.713 | 2.329 | 1.00 | 1.00 | 1.00 | 142 | 1.00 | 1.00 |
| pull | dynamic,256 | 1 | 2.531 | 2.148 | 1.07 | 1.07 | 1.08 | 153 | 1.000 | 1.000 |
| pull | dynamic,256 | 2 | 1.998 | 1.332 | 1.36 | 0.68 | 1.75 | 193 | 1.000 | 1.006 |
| pull | dynamic,256 | 4 | 1.793 | 1.347 | 1.51 | 0.38 | 1.73 | 215 | 1.011 | 1.028 |
| pull | guided | 1 | 2.391 | 2.056 | 1.13 | 1.13 | 1.13 | 161 | 1.000 | 1.000 |
| pull | guided | 2 | 2.043 | 1.968 | 1.33 | 0.66 | 1.18 | 189 | 1.132 | 1.170 |
| pull | guided | 4 | 1.903 | 1.571 | 1.43 | 0.36 | 1.48 | 203 | 1.118 | 1.140 |
| pull | static | 1 | 2.502 | 2.335 | 1.08 | 1.08 | 1.00 | 154 | 1.000 | 1.000 |
| pull | static | 2 | 2.222 | 1.733 | 1.22 | 0.61 | 1.34 | 174 | 1.385 | 1.510 |
| pull | static | 4 | 1.904 | 1.554 | 1.42 | 0.36 | 1.50 | 203 | 1.783 | 2.278 |
| push | dynamic,256 | 1 | 7.100 | 6.581 | 0.38 | 0.38 | 0.35 | 54 | 1.000 | 1.000 |
| push | dynamic,256 | 2 | 4.918 | 4.727 | 0.55 | 0.28 | 0.49 | 78 | 1.000 | 1.005 |
| push | dynamic,256 | 4 | 4.823 | 4.483 | 0.56 | 0.14 | 0.52 | 80 | 1.001 | 1.012 |
| push | guided | 1 | 7.373 | 6.266 | 0.37 | 0.37 | 0.37 | 52 | 1.000 | 1.000 |
| push | guided | 2 | 6.175 | 5.637 | 0.44 | 0.22 | 0.41 | 63 | 1.083 | 1.128 |
| push | guided | 4 | 6.099 | 5.215 | 0.44 | 0.11 | 0.45 | 63 | 1.139 | 1.141 |
| push | static | 1 | 8.602 | 6.959 | 0.32 | 0.32 | 0.33 | 45 | 1.000 | 1.000 |
| push | static | 2 | 7.806 | 6.238 | 0.35 | 0.17 | 0.37 | 49 | 1.402 | 1.511 |
| push | static | 4 | 6.144 | 5.171 | 0.44 | 0.11 | 0.45 | 63 | 1.795 | 2.279 |

## Schedule and chunk size (pull, median)

| schedule | p=2 time (s) | p=2 R | p=2 edges max/mean | p=4 time (s) | p=4 R | p=4 edges max/mean |
|---|---|---|---|---|---|---|
| static | 2.098 | 1.404 | 1.510 | 1.835 | 1.793 | 2.278 |
| static,64 | 2.093 | 1.366 | 1.511 | 1.876 | 1.739 | 2.279 |
| static,1024 | 2.020 | 1.383 | 1.511 | 2.028 | 1.761 | 2.280 |
| dynamic,1 | 2.785 | 1.000 | 1.006 | 2.619 | 1.003 | 1.014 |
| dynamic,16 | 1.891 | 1.001 | 1.001 | 1.705 | 1.011 | 1.013 |
| dynamic,64 | 1.625 | 1.000 | 1.008 | 1.700 | 1.006 | 1.023 |
| dynamic,256 | 1.922 | 1.001 | 1.010 | 1.618 | 1.005 | 1.015 |
| dynamic,1024 | 1.727 | 1.001 | 1.008 | 1.604 | 1.006 | 1.022 |
| dynamic,4096 | 1.650 | 1.000 | 1.009 | 1.995 | 1.004 | 1.026 |
| dynamic,16384 | 1.535 | 1.000 | 1.007 | 1.731 | 1.005 | 1.024 |
| guided | 1.916 | 1.030 | 1.043 | 2.070 | 1.121 | 1.219 |
| guided,64 | 1.935 | 1.049 | 1.043 | 2.122 | 1.092 | 1.137 |
| guided,256 | 1.933 | 1.100 | 1.128 | 1.973 | 1.061 | 1.136 |

## Cost of atomics (p = 1, static, median)

| version | time (s) | ns per edge per iteration |
|---|---|---|
| pull (no atomics, random reads) | 2.502 | 6.48 |
| push without atomic (random writes) | 2.829 | 7.33 |
| push with omp atomic (random atomic writes) | 6.979 | 18.08 |

Atomic overhead at p=1: 10.75 ns per edge (147% on top of the plain scatter); push/pull ratio 2.79.

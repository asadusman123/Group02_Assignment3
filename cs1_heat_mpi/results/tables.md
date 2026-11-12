## Verification: final grid compared byte-for-byte with serial (N=1024, T=500)

18 of 18 runs bit-identical to the serial output.

| decomposition | mode | p=1 | p=2 | p=4 |
|---|---|---|---|---|
| 1d | blocking | IDENTICAL | IDENTICAL | IDENTICAL |
| 1d | nbtest | IDENTICAL | IDENTICAL | IDENTICAL |
| 1d | nonblocking | IDENTICAL | IDENTICAL | IDENTICAL |
| 2d | blocking | IDENTICAL | IDENTICAL | IDENTICAL |
| 2d | nbtest | IDENTICAL | IDENTICAL | IDENTICAL |
| 2d | nonblocking | IDENTICAL | IDENTICAL | IDENTICAL |

## Strong scaling, N = 1024, T = 500, K = 50 (median of 5 runs; serial 1.050 s)

| dims | halo mode | p | median (s) | min (s) | S median | E median | S min | E min | compute (s) | halo comm (s) | allreduce (s) | halo values/rank/step |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial | - | 1 | 1.050 | 0.875 | 1.00 | 1.00 | 1.00 | 1.00 | 1.050 | 0 | 0 | 0 |
| 1x1 | blocking | 1 | 0.886 | 0.863 | 1.19 | 1.19 | 1.01 | 1.01 | 0.885 | 0.001 | 0.0000 | 0 |
| 1x1 | nbtest | 1 | 0.879 | 0.875 | 1.20 | 1.20 | 1.00 | 1.00 | 0.876 | 0.002 | 0.0000 | 0 |
| 1x1 | nonblocking | 1 | 0.898 | 0.871 | 1.17 | 1.17 | 1.00 | 1.00 | 0.896 | 0.002 | 0.0000 | 0 |
| 2x1 | blocking | 2 | 0.924 | 0.823 | 1.14 | 0.57 | 1.06 | 0.53 | 0.836 | 0.089 | 0.0054 | 1,024 |
| 2x1 | nbtest | 2 | 0.818 | 0.806 | 1.28 | 0.64 | 1.09 | 0.54 | 0.781 | 0.041 | 0.0087 | 1,024 |
| 2x1 | nonblocking | 2 | 0.836 | 0.814 | 1.26 | 0.63 | 1.08 | 0.54 | 0.783 | 0.063 | 0.0002 | 1,024 |
| 2x2 | blocking | 4 | 2.672 | 2.385 | 0.39 | 0.10 | 0.37 | 0.09 | 0.538 | 2.155 | 0.0367 | 1,024 |
| 2x2 | nbtest | 4 | 1.389 | 1.365 | 0.76 | 0.19 | 0.64 | 0.16 | 0.607 | 0.781 | 0.0423 | 1,024 |
| 2x2 | nonblocking | 4 | 1.829 | 1.696 | 0.57 | 0.14 | 0.52 | 0.13 | 0.588 | 1.280 | 0.0342 | 1,024 |
| 4x1 | blocking | 4 | 2.055 | 1.996 | 0.51 | 0.13 | 0.44 | 0.11 | 0.578 | 1.586 | 0.0335 | 2,048 |
| 4x1 | nbtest | 4 | 1.203 | 1.103 | 0.87 | 0.22 | 0.79 | 0.20 | 0.635 | 0.601 | 0.0376 | 2,048 |
| 4x1 | nonblocking | 4 | 1.224 | 1.221 | 0.86 | 0.21 | 0.72 | 0.18 | 0.618 | 0.674 | 0.0332 | 2,048 |

## Strong scaling, N = 4096, T = 500, K = 50 (median of 3 runs; serial 13.792 s)

| dims | halo mode | p | median (s) | min (s) | S median | E median | S min | E min | compute (s) | halo comm (s) | allreduce (s) | halo values/rank/step |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial | - | 1 | 13.792 | 13.762 | 1.00 | 1.00 | 1.00 | 1.00 | 13.792 | 0 | 0 | 0 |
| 1x1 | blocking | 1 | 13.399 | 13.398 | 1.03 | 1.03 | 1.03 | 1.03 | 13.398 | 0.001 | 0.0001 | 0 |
| 1x1 | nbtest | 1 | 14.123 | 13.579 | 0.98 | 0.98 | 1.01 | 1.01 | 14.119 | 0.003 | 0.0000 | 0 |
| 1x1 | nonblocking | 1 | 13.627 | 13.480 | 1.01 | 1.01 | 1.02 | 1.02 | 13.624 | 0.002 | 0.0000 | 0 |
| 2x1 | blocking | 2 | 12.963 | 12.626 | 1.06 | 0.53 | 1.09 | 0.54 | 12.505 | 0.434 | 0.0472 | 4,096 |
| 2x1 | nbtest | 2 | 13.037 | 12.602 | 1.06 | 0.53 | 1.09 | 0.55 | 12.949 | 0.085 | 0.0582 | 4,096 |
| 2x1 | nonblocking | 2 | 13.129 | 12.538 | 1.05 | 0.53 | 1.10 | 0.55 | 12.827 | 0.366 | 0.0591 | 4,096 |
| 2x2 | blocking | 4 | 15.141 | 13.788 | 0.91 | 0.23 | 1.00 | 0.25 | 10.836 | 4.462 | 0.0636 | 4,096 |
| 2x2 | nbtest | 4 | 13.617 | 13.572 | 1.01 | 0.25 | 1.01 | 0.25 | 12.802 | 1.110 | 0.1562 | 4,096 |
| 2x2 | nonblocking | 4 | 12.916 | 12.698 | 1.07 | 0.27 | 1.08 | 0.27 | 11.011 | 1.888 | 0.0893 | 4,096 |
| 4x1 | blocking | 4 | 13.569 | 13.502 | 1.02 | 0.25 | 1.02 | 0.25 | 10.990 | 2.842 | 0.0752 | 8,192 |
| 4x1 | nbtest | 4 | 12.701 | 12.378 | 1.09 | 0.27 | 1.11 | 0.28 | 12.273 | 0.580 | 0.1682 | 8,192 |
| 4x1 | nonblocking | 4 | 13.085 | 12.840 | 1.05 | 0.26 | 1.07 | 0.27 | 12.181 | 1.017 | 0.2814 | 8,192 |

## Surface-to-volume per rank per step (formula vs. what the code exchanged)

| N | p | decomposition | cells/rank N^2/p | halo values/rank (formula) | halo (measured, max rank) | halo / cells |
|---|---|---|---|---|---|---|
| 1024 | 2 | 2x1 (1-D rows) | 524,288 | 2,048 | 1,024 | 0.0039 |
| 1024 | 4 | 2x2 (2-D blocks) | 262,144 | 2,048 | 1,024 | 0.0078 |
| 1024 | 4 | 4x1 (1-D rows) | 262,144 | 2,048 | 2,048 | 0.0078 |
| 4096 | 2 | 2x1 (1-D rows) | 8,388,608 | 8,192 | 4,096 | 0.0010 |
| 4096 | 4 | 2x2 (2-D blocks) | 4,194,304 | 8,192 | 4,096 | 0.0020 |
| 4096 | 4 | 4x1 (1-D rows) | 4,194,304 | 8,192 | 8,192 | 0.0020 |

Formula: 1-D interior block receives 2 rows of N values; 2-D interior block receives 4 edges of N/sqrt(p). Edge blocks receive fewer (no neighbour beyond the boundary), which is why the measured maximum can be below the interior-block formula at small p.

## Blocking vs non-blocking (overlap), median times

| N | dims | p | blocking total (s) | blocking halo comm (s) | nonblocking total (s) | nonblocking wait (s) | nbtest total (s) | nbtest comm (s) | time saved by nonblocking |
|---|---|---|---|---|---|---|---|---|---|
| 1024 | 1x1 | 1 | 0.886 | 0.001 | 0.898 | 0.002 | 0.879 | 0.002 | -1.4% |
| 1024 | 2x1 | 2 | 0.924 | 0.089 | 0.836 | 0.063 | 0.818 | 0.041 | +9.5% |
| 1024 | 2x2 | 4 | 2.672 | 2.155 | 1.829 | 1.280 | 1.389 | 0.781 | +31.5% |
| 1024 | 4x1 | 4 | 2.055 | 1.586 | 1.224 | 0.674 | 1.203 | 0.601 | +40.4% |
| 4096 | 1x1 | 1 | 13.399 | 0.001 | 13.627 | 0.002 | 14.123 | 0.003 | -1.7% |
| 4096 | 2x1 | 2 | 12.963 | 0.434 | 13.129 | 0.366 | 13.037 | 0.085 | -1.3% |
| 4096 | 2x2 | 4 | 15.141 | 4.462 | 12.916 | 1.888 | 13.617 | 1.110 | +14.7% |
| 4096 | 4x1 | 4 | 13.569 | 2.842 | 13.085 | 1.017 | 12.701 | 0.580 | +3.6% |

## Model T_p ~ T_serial/p + T_comm  (2-D, non-blocking; T_comm = measured halo + allreduce)

| N | p | measured T_p (s) | T_serial/p (s) | T_comm (s) | model (s) | E median | E < 70%? |
|---|---|---|---|---|---|---|---|
| 1024 | 1 | 0.898 | 1.050 | 0.002 | 1.052 | 1.17 | no |
| 1024 | 2 | 0.836 | 0.525 | 0.063 | 0.588 | 0.63 | yes |
| 1024 | 4 | 1.829 | 0.263 | 1.314 | 1.577 | 0.14 | yes |
| 4096 | 1 | 13.627 | 13.792 | 0.002 | 13.794 | 1.01 | no |
| 4096 | 2 | 13.129 | 6.896 | 0.425 | 7.321 | 0.53 | yes |
| 4096 | 4 | 12.916 | 3.448 | 1.977 | 5.425 | 0.27 | yes |

## Convergence-check cost: K = 1 vs K = 50 (median)

| N | version | p | time K=1 (s) | time K=50 (s) | K=1 / K=50 | compute K=1 | compute K=50 | allreduce K=1 | allreduce K=50 |
|---|---|---|---|---|---|---|---|---|---|
| 1024 | serial | 1 | 2.029 | 1.344 | 1.51 | 2.029 | 1.344 | 0.0000 | 0.0000 |
| 1024 | parallel | 2 | 1.838 | 0.901 | 2.04 | 1.606 | 0.843 | 0.0197 | 0.0025 |
| 1024 | parallel | 4 | 4.676 | 2.905 | 1.61 | 1.329 | 0.740 | 1.8307 | 0.0386 |
| 4096 | serial | 1 | 34.268 | 17.116 | 2.00 | 34.268 | 17.116 | 0.0000 | 0.0000 |
| 4096 | parallel | 2 | 23.342 | 15.382 | 1.52 | 22.239 | 14.931 | 0.1023 | 0.0605 |
| 4096 | parallel | 4 | 26.491 | 14.776 | 1.79 | 21.002 | 11.923 | 2.7728 | 0.0947 |

## Weak scaling (2-D, non-blocking): N_p = N0*sqrt(p), cells per rank constant

| p | N_p | cells/rank | T_p median (s) | T_p min (s) | serial on N_p (s) | weak efficiency T_1/T_p | speedup vs serial on same N |
|---|---|---|---|---|---|---|---|
| 1 | 2048 | 4,194,304 | 3.858 | 3.704 | 3.837 | 1.00 | 0.99 |
| 2 | 2896 | 4,193,408 | 7.058 | 6.913 | 10.764 | 0.55 | 1.53 |
| 4 | 4096 | 4,194,304 | 19.196 | 14.322 | 17.849 | 0.20 | 0.93 |

## Strong scaling (M fixed)

Median of 9 interleaved runs. Speedup is relative to the serial baseline (T1 = 4.362 s). p > 2 uses hyper-threading.

| version | p | median time (s) | min | max | spread max/min | S(p) median | E(p) median | S(p) from min | E(p) from min |
|---|---|---|---|---|---|---|---|---|---|
| serial | 1 | 4.362 | 4.261 | 5.385 | 1.26 | 1.00 | 1.00 | 1.00 | 1.00 |
| parallel | 1 | 4.462 | 4.176 | 4.621 | 1.11 | 0.98 | 0.98 | 1.02 | 1.02 |
| parallel | 2 | 3.110 | 3.051 | 4.475 | 1.47 | 1.40 | 0.70 | 1.40 | 0.70 |
| parallel | 4 | 2.568 | 2.367 | 3.488 | 1.47 | 1.70 | 0.42 | 1.80 | 0.45 |

Experimentally determined serial fraction (Karp-Flatt), f = (1/S - 1/p) / (1 - 1/p):

- p = 2: f = 0.426 (median-based), 0.432 (min-based)
- p = 4: f = 0.452 (median-based), 0.407 (min-based)

Thread placement: OMP_PLACES=cores, OMP_PROC_BIND=close -> p <= 2 uses one thread per physical core; p = 4 puts two hyper-threads on each core.

## Fixed-time runs (M_p chosen to finish in the serial best time)

SE depends only on M and the seed, so it is identical in every round. Target time = serial minimum = 4.261 s.

| p | samples M | SE | SE_1 / SE_p | sqrt(M_p / M_1) | median time (s) | min time (s) | price | deviation from BS (in SE) |
|---|---|---|---|---|---|---|---|---|
| 1 | 102,043,462 | 0.0014574 | 1.000 | 1.000 | 5.023 | 4.376 | 10.451644 | 0.73 |
| 2 | 139,666,278 | 0.0012457 | 1.170 | 1.170 | 4.469 | 4.391 | 10.451464 | 0.71 |
| 4 | 180,021,974 | 0.0010971 | 1.328 | 1.328 | 5.045 | 4.369 | 10.452038 | 1.33 |

## RNG strategies

| mode | p | M | time (s) | M samples/s | price | reported SE | true SE* |
|---|---|---|---|---|---|---|---|
| jump | 4 | 100,000,000 | 2.468 | 40.52 | 10.451660 | 0.001472 | 0.001472 |
| shared | 4 | 20,000,000 | 2.234 | 8.95 | 10.452101 | 0.003292 | 0.003292 |
| sameseed | 4 | 100,000,000 | 2.437 | 41.03 | 10.452449 | 0.001472 | 0.002945 |

*sameseed: all p threads replay one stream, so only M/p samples are independent and the true SE is sqrt(p) times the reported one.

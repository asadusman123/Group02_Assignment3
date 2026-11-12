## Strong scaling (M fixed)

Median of 9 interleaved runs. Speedup is relative to the serial baseline (T1 = 10.220 s). p > 2 uses hyper-threading.

| version | p | median time (s) | min | max | spread max/min | S(p) median | E(p) median | S(p) from min | E(p) from min |
|---|---|---|---|---|---|---|---|---|---|
| serial | 1 | 10.220 | 4.504 | 17.153 | 3.81 | 1.00 | 1.00 | 1.00 | 1.00 |
| parallel | 1 | 6.272 | 4.634 | 11.508 | 2.48 | 1.63 | 1.63 | 0.97 | 0.97 |
| parallel | 2 | 4.758 | 3.211 | 8.204 | 2.55 | 2.15 | 1.07 | 1.40 | 0.70 |
| parallel | 4 | 3.738 | 2.565 | 5.334 | 2.08 | 2.73 | 0.68 | 1.76 | 0.44 |

Experimentally determined serial fraction (Karp-Flatt), f = (1/S - 1/p) / (1 - 1/p):

- p = 2: f = -0.069 (median-based), 0.426 (min-based)
- p = 4: f = 0.154 (median-based), 0.426 (min-based)

## Fixed-time runs (accuracy per second)

| p | samples M | time (s) | price | std. error | deviation from BS (in SE) |
|---|---|---|---|---|---|
| 1 | 162,938,100 | 21.267 | 10.451436 | 0.001153 | 0.74 |
| 2 | 214,779,944 | 10.220 | 10.450896 | 0.001004 | 0.31 |
| 4 | 273,377,754 | 10.402 | 10.451716 | 0.000890 | 1.27 |

## RNG strategies

| mode | p | M | time (s) | M samples/s | price | reported SE | true SE* |
|---|---|---|---|---|---|---|---|
| jump | 4 | 100,000,000 | 6.378 | 15.68 | 10.451660 | 0.001472 | 0.001472 |
| shared | 4 | 20,000,000 | 5.244 | 3.81 | 10.452101 | 0.003292 | 0.003292 |
| sameseed | 4 | 100,000,000 | 7.283 | 13.73 | 10.452449 | 0.001472 | 0.002945 |

*sameseed: all p threads replay one stream, so only M/p samples are independent and the true SE is sqrt(p) times the reported one.

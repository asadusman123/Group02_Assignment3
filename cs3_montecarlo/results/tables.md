## Strong scaling (M fixed)

Median of 5 interleaved runs. Speedup is relative to the serial baseline (T1 = 8.215 s). p > 2 uses hyper-threading.

| version | p | median time (s) | min | max | speedup S(p) | efficiency E(p) |
|---|---|---|---|---|---|---|
| serial | 1 | 8.215 | 4.590 | 16.688 | 1.00 | 1.00 |
| parallel | 1 | 9.200 | 4.379 | 9.889 | 0.89 | 0.89 |
| parallel | 2 | 3.524 | 3.168 | 9.346 | 2.33 | 1.17 |
| parallel | 4 | 3.109 | 2.634 | 5.114 | 2.64 | 0.66 |

Experimentally determined serial fraction (Karp-Flatt), f = (1/S - 1/p) / (1 - 1/p):

- p = 2: f = -0.142
- p = 4: f = 0.171

## Fixed-time runs (accuracy per second)

| p | samples M | time (s) | price | std. error | deviation from BS (in SE) |
|---|---|---|---|---|---|
| 1 | 89,296,454 | 4.354 | 10.452586 | 0.001558 | 1.29 |
| 2 | 233,117,466 | 12.506 | 10.450764 | 0.000964 | 0.19 |
| 4 | 264,240,634 | 16.706 | 10.451667 | 0.000906 | 1.20 |

## RNG strategies

| mode | p | M | time (s) | M samples/s | price | reported SE | true SE* |
|---|---|---|---|---|---|---|---|
| jump | 4 | 100,000,000 | 6.355 | 15.74 | 10.451660 | 0.001472 | 0.001472 |
| shared | 4 | 20,000,000 | 4.528 | 4.42 | 10.452101 | 0.003292 | 0.003292 |
| sameseed | 4 | 100,000,000 | 2.951 | 33.89 | 10.452449 | 0.001472 | 0.002945 |

*sameseed: all p threads replay one stream, so only M/p samples are independent and the true SE is sqrt(p) times the reported one.

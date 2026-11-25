## Ping-pong: T(m) = alpha + beta*m  (best of 3 rounds, one-way time)

| message | one-way time (us) | bandwidth (GB/s) | model alpha+beta*m (us) |
|---|---|---|---|
| 1 B | 0.27 | 0.00 | 0.29 |
| 4 B | 0.29 | 0.01 | 0.29 |
| 16 B | 0.29 | 0.05 | 0.29 |
| 64 B | 0.30 | 0.21 | 0.30 |
| 256 B | 0.36 | 0.71 | 0.34 |
| 1,024 B | 0.60 | 1.70 | 0.51 |
| 4,096 B | 3.88 | 1.06 | 1.16 |
| 16,384 B | 6.57 | 2.49 | 3.77 |
| 65,536 B | 11.93 | 5.49 | 14.21 |
| 262,144 B | 38.03 | 6.89 | 56.00 |
| 1,048,576 B | 176.42 | 5.94 | 223.12 |
| 4,194,304 B | 938.72 | 4.47 | 891.62 |
| 16,777,216 B | 3546.72 | 4.73 | 3565.61 |
| 67,108,864 B | 14445.09 | 4.65 | 14261.56 |

alpha = 0.288 us (mean of messages <= 64 B); beta = 0.2125 ns/B (= 4.71 GB/s, fit 64 KiB-16 MiB); for a 64 MiB message beta_DRAM = 0.2152 ns/B (= 4.65 GB/s).

## Load balance after redistribution: max/mean elements per rank vs oversampling ov (ov*p - 1 samples per rank)

| p | distribution | splitter rule | ov=1 | ov=4 | ov=16 | ov=64 | ov=256 |
|---|---|---|---|---|---|---|---|
| 2 | uniform | key only | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| 2 | uniform | key+rank+pos | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| 2 | zipf | key only | 1.037 | 1.037 | 1.037 | 1.037 | 1.037 |
| 2 | zipf | key+rank+pos | 1.019 | 1.019 | 1.019 | 1.003 | 1.001 |
| 4 | uniform | key only | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| 4 | uniform | key+rank+pos | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| 4 | zipf | key only | 1.532 | 1.532 | 1.532 | 1.532 | 1.532 |
| 4 | zipf | key+rank+pos | 1.073 | 1.040 | 1.007 | 1.008 | 1.001 |

Final elements per rank at ov = 1 and the largest ov:

| p | distribution | splitter rule | ov | counts per rank |
|---|---|---|---|---|
| 2 | uniform | key only | 1 | 50005926, 49994074 |
| 2 | uniform | key only | 256 | 50005926, 49994074 |
| 2 | uniform | key+rank+pos | 1 | 50005926, 49994074 |
| 2 | uniform | key+rank+pos | 256 | 50005926, 49994074 |
| 2 | zipf | key only | 1 | 51851675, 48148325 |
| 2 | zipf | key only | 256 | 51851675, 48148325 |
| 2 | zipf | key+rank+pos | 1 | 50929026, 49070974 |
| 2 | zipf | key+rank+pos | 256 | 50050119, 49949881 |
| 4 | uniform | key only | 1 | 24997591, 25002068, 25003180, 24997161 |
| 4 | uniform | key only | 256 | 24997591, 25002068, 25003180, 24997161 |
| 4 | uniform | key+rank+pos | 1 | 24997591, 25002068, 25003180, 24997161 |
| 4 | uniform | key+rank+pos | 256 | 24997591, 25002068, 25003180, 24997161 |
| 4 | zipf | key only | 1 | 38304375, 13546075, 23371255, 24778295 |
| 4 | zipf | key only | 256 | 38304375, 13546075, 23371255, 24778295 |
| 4 | zipf | key+rank+pos | 1 | 25399423, 22601964, 26814618, 25183995 |
| 4 | zipf | key+rank+pos | 256 | 24984383, 25034323, 24973856, 25007438 |

## uniform, local sort = qsort: time and phase breakdown (median of 3 runs; N = 100,000,000)

| version | p | total (s) | min (s) | speedup | efficiency | local sort | sampling | partition | alltoall(v) | merge | max/mean count | verified |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial (1 process) | 1 | 30.484 | 23.826 | 1.00 | 1.00 | 30.484 | – | – | – | – | – | PASS |
| sample sort | 1 | 33.904 | 22.374 | 0.90 | 0.90 | 33.623 | 0.000 | 0.000 | 0.287 | 0.000 | 1.000 | PASS |
| sample sort | 2 | 19.973 | 19.632 | 1.53 | 0.76 | 19.198 | 0.000 | 0.000 | 0.246 | 0.635 | 1.000 | PASS |
| sample sort | 4 | 19.649 | 11.457 | 1.55 | 0.39 | 18.214 | 0.001 | 0.000 | 0.171 | 0.583 | 1.000 | PASS |

## uniform, local sort = radix: time and phase breakdown (median of 3 runs; N = 100,000,000)

| version | p | total (s) | min (s) | speedup | efficiency | local sort | sampling | partition | alltoall(v) | merge | max/mean count | verified |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial (1 process) | 1 | 2.219 | 1.428 | 1.00 | 1.00 | 2.219 | – | – | – | – | – | PASS |
| sample sort | 1 | 2.651 | 1.721 | 0.84 | 0.84 | 2.107 | 0.000 | 0.000 | 0.351 | 0.000 | 1.000 | PASS |
| sample sort | 2 | 2.598 | 2.347 | 0.85 | 0.43 | 1.534 | 0.000 | 0.000 | 0.301 | 0.763 | 1.000 | PASS |
| sample sort | 4 | 3.105 | 2.957 | 0.71 | 0.18 | 1.744 | 0.020 | 0.011 | 0.342 | 1.321 | 1.000 | PASS |

## zipf, local sort = qsort: time and phase breakdown (median of 3 runs; N = 100,000,000)

| version | p | total (s) | min (s) | speedup | efficiency | local sort | sampling | partition | alltoall(v) | merge | max/mean count | verified |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial (1 process) | 1 | 22.563 | 19.265 | 1.00 | 1.00 | 22.563 | – | – | – | – | – | PASS |
| sample sort | 1 | 18.637 | 13.406 | 1.21 | 1.21 | 18.350 | 0.000 | 0.000 | 0.303 | 0.000 | 1.000 | PASS |
| sample sort | 2 | 15.546 | 13.931 | 1.45 | 0.73 | 14.999 | 0.000 | 0.000 | 0.218 | 0.328 | 1.019 | PASS |
| sample sort | 4 | 12.047 | 7.667 | 1.87 | 0.47 | 11.406 | 0.000 | 0.000 | 0.239 | 0.402 | 1.073 | PASS |

## zipf, local sort = radix: time and phase breakdown (median of 3 runs; N = 100,000,000)

| version | p | total (s) | min (s) | speedup | efficiency | local sort | sampling | partition | alltoall(v) | merge | max/mean count | verified |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| serial (1 process) | 1 | 2.176 | 1.878 | 1.00 | 1.00 | 2.176 | – | – | – | – | – | PASS |
| sample sort | 1 | 2.087 | 1.986 | 1.04 | 1.04 | 1.773 | 0.000 | 0.000 | 0.314 | 0.000 | 1.000 | PASS |
| sample sort | 2 | 1.863 | 1.735 | 1.17 | 0.58 | 1.378 | 0.000 | 0.000 | 0.239 | 0.246 | 1.019 | PASS |
| sample sort | 4 | 1.769 | 1.342 | 1.23 | 0.31 | 0.936 | 0.001 | 0.000 | 0.334 | 0.410 | 1.073 | PASS |

## Alltoallv: model (p-1)*alpha + beta*bytes vs measured (median)

| dist | sort | p | max bytes sent by one rank to others | predicted, beta fit (s) | predicted, beta_DRAM (s) | measured (s) |
|---|---|---|---|---|---|---|
| uniform | qsort | 2 | 95.4 MiB | 0.021 | 0.022 | 0.246 |
| uniform | qsort | 4 | 71.5 MiB | 0.016 | 0.016 | 0.171 |
| uniform | radix | 2 | 95.4 MiB | 0.021 | 0.022 | 0.301 |
| uniform | radix | 4 | 71.5 MiB | 0.016 | 0.016 | 0.342 |
| zipf | qsort | 2 | 95.4 MiB | 0.021 | 0.022 | 0.218 |
| zipf | qsort | 4 | 82.4 MiB | 0.018 | 0.019 | 0.239 |
| zipf | radix | 2 | 95.4 MiB | 0.021 | 0.022 | 0.239 |
| zipf | radix | 4 | 82.4 MiB | 0.018 | 0.019 | 0.334 |

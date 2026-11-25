/*
 * ss_common.h - shared code for Case Study 5 (sample sort).
 *
 *   - key generation: uniform 32-bit, or Zipf(s = 1.5) over 2^20 values
 *     (value 0 alone is ~38% of all keys: heavy duplicates)
 *   - local sorts: qsort (as the brief asks) and an LSD radix sort (optional)
 *   - multiset fingerprint: sum of a hash of every key (mod 2^64)
 */
#ifndef SS_COMMON_H
#define SS_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ZIPF_S     1.5
#define ZIPF_K     (1u << 20)

static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* keys for one shard: seed = base_seed * 1000003 + shard id */
static void gen_keys(uint32_t *a, uint64_t n, int zipf, uint64_t base_seed, int shard)
{
    uint64_t x = base_seed * 1000003ULL + (uint64_t)shard;
    if (!zipf) {
        for (uint64_t i = 0; i < n; i++) a[i] = (uint32_t)(splitmix64(&x) >> 32);
        return;
    }
    double *cdf = malloc(ZIPF_K * sizeof(double));
    double s = 0.0;
    for (uint32_t k = 0; k < ZIPF_K; k++) { s += pow(k + 1.0, -ZIPF_S); cdf[k] = s; }
    for (uint32_t k = 0; k < ZIPF_K; k++) cdf[k] /= s;
    for (uint64_t i = 0; i < n; i++) {
        const double u = (double)(splitmix64(&x) >> 11) * 0x1.0p-53;
        uint32_t lo = 0, hi = ZIPF_K - 1;               /* first k with cdf[k] > u */
        while (lo < hi) { const uint32_t mid = (lo + hi) / 2; if (cdf[mid] > u) hi = mid; else lo = mid + 1; }
        a[i] = lo;
    }
    free(cdf);
}

static int cmp_u32(const void *p, const void *q)
{
    const uint32_t a = *(const uint32_t *)p, b = *(const uint32_t *)q;
    return (a > b) - (a < b);
}

/* LSD radix sort, 4 passes of 8 bits; tmp must hold n keys */
static void radix_sort_u32(uint32_t *a, uint32_t *tmp, uint64_t n)
{
    for (int sh = 0; sh < 32; sh += 8) {
        uint64_t cnt[256] = { 0 };
        for (uint64_t i = 0; i < n; i++) cnt[(a[i] >> sh) & 255]++;
        uint64_t sum = 0;
        for (int b = 0; b < 256; b++) { const uint64_t c = cnt[b]; cnt[b] = sum; sum += c; }
        for (uint64_t i = 0; i < n; i++) tmp[cnt[(a[i] >> sh) & 255]++] = a[i];
        uint32_t *t = a; a = tmp; tmp = t;
    }   /* 4 passes: result is back in the original array */
}

static inline void local_sort(uint32_t *a, uint64_t n, int radix)
{
    if (radix) {
        uint32_t *tmp = malloc((n ? n : 1) * sizeof(uint32_t));
        radix_sort_u32(a, tmp, n);
        free(tmp);
    } else {
        qsort(a, n, sizeof(uint32_t), cmp_u32);
    }
}

static inline uint64_t fingerprint(const uint32_t *a, uint64_t n)
{
    uint64_t f = 0;
    for (uint64_t i = 0; i < n; i++) { uint64_t x = a[i]; f += splitmix64(&x); }
    return f;
}

static inline int is_sorted_u32(const uint32_t *a, uint64_t n)
{
    for (uint64_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

#endif

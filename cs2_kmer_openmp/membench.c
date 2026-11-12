/*
 * membench.c - small memory-system probe used to interpret CS2 (and CS4).
 *
 * Usage: OMP_NUM_THREADS=p ./membench
 *
 *  1. Sequential read bandwidth: p threads sum disjoint slices of a 512 MiB
 *     array (each thread first-touches its own slice). Best of 5 passes.
 *  2. Random read-modify-write rate: each thread increments random entries of
 *     its OWN table of 32-bit counters, exactly the access pattern of a
 *     private k-mer histogram. Two table sizes: 256 KiB (= 4^8 bins, fits in
 *     L2) and 64 MiB (= 4^12 bins, far larger than the 3 MiB L3).
 *     Indices come from a xorshift generator, so they are independent and the
 *     CPU can overlap many cache misses (a throughput, not a latency, test).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <omp.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }

static double random_rmw(uint64_t table_entries, uint64_t updates_per_thread)
{
    const int p = omp_get_max_threads();
    uint32_t **tab = malloc(p * sizeof(uint32_t *));
    double best = 1e30;
    uint64_t sink = 0;
    #pragma omp parallel
    {
        const int t = omp_get_thread_num();
        tab[t] = calloc(table_entries, sizeof(uint32_t));
    }
    for (int rep = 0; rep < 3; rep++) {
        const double t0 = now();
        #pragma omp parallel reduction(+:sink)
        {
            const int t = omp_get_thread_num();
            uint32_t *T = tab[t];
            uint64_t x = 0x9E3779B97F4A7C15ULL ^ (uint64_t)(t + 1) * 0xD1B54A32D192ED03ULL;
            const uint64_t mask = table_entries - 1;      /* power of two */
            for (uint64_t i = 0; i < updates_per_thread; i++) {
                x ^= x << 13; x ^= x >> 7; x ^= x << 17;
                T[x & mask]++;
            }
            sink += T[x & mask];
        }
        const double dt = now() - t0;
        if (dt < best) best = dt;
    }
    for (int t = 0; t < p; t++) free(tab[t]);
    free(tab);
    if (sink == 42) printf(" ");
    return (double)updates_per_thread * p / best;        /* updates per second */
}

int main(void)
{
    const int p = omp_get_max_threads();

    /* ---- 1. sequential read bandwidth ---- */
    const uint64_t n = (512ULL << 20) / sizeof(uint64_t);
    uint64_t *a = malloc(n * sizeof(uint64_t));
    #pragma omp parallel for schedule(static)
    for (uint64_t i = 0; i < n; i++) a[i] = i;
    double best = 1e30;
    uint64_t sum = 0;
    for (int rep = 0; rep < 5; rep++) {
        uint64_t s = 0;
        const double t0 = now();
        #pragma omp parallel for schedule(static) reduction(+:s)
        for (uint64_t i = 0; i < n; i++) s += a[i];
        const double dt = now() - t0;
        if (dt < best) best = dt;
        sum += s;
    }
    const double bw = n * sizeof(uint64_t) / best / 1e9;
    free(a);

    /* ---- 2. random read-modify-write rates ---- */
    const double r_l2  = random_rmw(1ULL << 16, 200000000ULL / p);   /* 256 KiB */
    const double r_dram = random_rmw(1ULL << 24, 50000000ULL / p);   /* 64 MiB  */

    printf("threads=%d  read bandwidth = %.2f GB/s   random RMW: 256KiB table %.0f M/s, 64MiB table %.0f M/s  (chk %llu)\n",
           p, bw, r_l2 / 1e6, r_dram / 1e6, (unsigned long long)(sum & 1));
    printf("RESULT membench p=%d read_gbs=%.3f rmw_256k_mps=%.1f rmw_64m_mps=%.1f\n",
           p, bw, r_l2 / 1e6, r_dram / 1e6);
    return 0;
}

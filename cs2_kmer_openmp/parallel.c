/*
 * parallel.c - OpenMP k-mer counters (Case Study 2).
 *
 * Usage: OMP_NUM_THREADS=p ./parallel VERSION [k] [L] [uniform|skewed] [seed] [verify|noverify]
 *        defaults: k = 12, L = 1e8, uniform, seed = 2026, verify
 *
 * VERSION
 *   A      parallel for, shared histogram, NO protection   (wrong on purpose: data race)
 *   B      parallel for, shared histogram, #pragma omp atomic
 *   C      parallel for, one private histogram per thread, parallel merge
 *   N      like C, but WRONG chunk boundaries: the SEQUENCE is cut into p
 *          disjoint pieces and each thread counts only k-mers that fit
 *          entirely inside its piece -> (p-1)(k-1) boundary k-mers are lost
 *   fs     false-sharing demo: k = 1 (4 bins); every thread's 4 counters sit
 *          next to each other in one shared array (32 B per thread)
 *   fspad  same as fs but each thread's counters padded to 128 B
 *
 * Decomposition (A, B, C): the L-k+1 k-mer START positions are split into p
 * contiguous chunks, one per loop iteration of "omp parallel for
 * schedule(static)". Each chunk primes its rolling window with the k-1 bases
 * at its start (reading into the neighbour's region, read-only), so a k-mer
 * that spans two chunks is counted exactly once - by the chunk that owns
 * its start position.
 *
 * "verify" recomputes the histogram serially (untimed) and compares every bin.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <omp.h>
#include "kmer_common.h"

#define INC_PRIV(x) (mine[(x)]++)
#define FS_STRIDE_UNPADDED 4    /* 4 x 8 B = 32 B per thread: 2 threads per 64 B line   */
#define FS_STRIDE_PADDED   16   /* 16 x 8 B = 128 B per thread: own line (+ prefetch pair) */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s A|B|C|N|fs|fspad [k] [L] [uniform|skewed] [seed] [verify|noverify]\n", argv[0]);
        return 1;
    }
    const char *ver = argv[1];
    const int is_fs = !strcmp(ver, "fs") || !strcmp(ver, "fspad");
    if (strcmp(ver, "A") && strcmp(ver, "B") && strcmp(ver, "C") && strcmp(ver, "N") && !is_fs) {
        fprintf(stderr, "VERSION must be A, B, C, N, fs or fspad\n");
        return 1;
    }
    args_t a = parse_args(argc, argv, 2);
    if (is_fs) a.k = 1;
    const int verify = !(argc > 6 && !strcmp(argv[6], "noverify"));
    init_encoding();

    double tg = wall_time();
    char *seq = make_sequence(a.L, a.seed, a.skewed);
    tg = wall_time() - tg;

    const int p = omp_get_max_threads();
    const uint64_t nbins  = 1ULL << (2 * a.k);
    const uint64_t nkmers = a.L - a.k + 1;
    count_t *hist = calloc(nbins, sizeof(count_t));
    if (!hist) { fprintf(stderr, "out of memory\n"); return 1; }

    double t_merge = 0.0;
    uint64_t extra_bytes = 0;
    const double t0 = wall_time();

    if (!strcmp(ver, "A")) {
        /* ---- Version A: data race on hist[] ---- */
        #pragma omp parallel for schedule(static)
        for (int c = 0; c < p; c++) {
            const uint64_t lo = (uint64_t)c * nkmers / p, hi = (uint64_t)(c + 1) * nkmers / p;
#define INC_RACE(x) (hist[(x)]++)
            COUNT_RANGE(seq, a.k, lo, hi, INC_RACE);
        }
    } else if (!strcmp(ver, "B")) {
        /* ---- Version B: every increment is an atomic read-modify-write ---- */
        #pragma omp parallel for schedule(static)
        for (int c = 0; c < p; c++) {
            const uint64_t lo = (uint64_t)c * nkmers / p, hi = (uint64_t)(c + 1) * nkmers / p;
#define INC_ATOMIC(x) do { _Pragma("omp atomic") hist[(x)]++; } while (0)
            COUNT_RANGE(seq, a.k, lo, hi, INC_ATOMIC);
        }
    } else if (!strcmp(ver, "N")) {
        /* ---- naive boundaries: split the L characters, not the k-mer starts ---- */
        count_t **priv = malloc(p * sizeof(count_t *));
        #pragma omp parallel
        {
            const int t = omp_get_thread_num();
            priv[t] = calloc(nbins, sizeof(count_t));
            count_t *mine = priv[t];
            #pragma omp for schedule(static)
            for (int c = 0; c < p; c++) {
                const uint64_t clo = (uint64_t)c * a.L / p, chi = (uint64_t)(c + 1) * a.L / p;
                /* only windows fully inside [clo, chi): starts clo .. chi-k */
                if (chi - clo >= (uint64_t)a.k)
                    COUNT_RANGE(seq, a.k, clo, chi - a.k + 1, INC_PRIV);
            }
            #pragma omp for schedule(static)
            for (uint64_t i = 0; i < nbins; i++) {
                count_t s = 0;
                for (int q = 0; q < p; q++) s += priv[q][i];
                hist[i] = s;
            }
        }
        for (int q = 0; q < p; q++) free(priv[q]);
        free(priv);
    } else if (!strcmp(ver, "C")) {
        /* ---- Version C: private histograms, then a parallel merge ---- */
        count_t **priv = malloc(p * sizeof(count_t *));
        extra_bytes = (uint64_t)p * nbins * sizeof(count_t);
        double t_count_end = 0.0;
        #pragma omp parallel
        {
            const int t = omp_get_thread_num();
            /* each thread allocates and zeroes its own copy (first touch) */
            priv[t] = calloc(nbins, sizeof(count_t));
            if (!priv[t]) { fprintf(stderr, "out of memory (private hist)\n"); exit(1); }
            count_t *mine = priv[t];

            #pragma omp for schedule(static)
            for (int c = 0; c < p; c++) {
                const uint64_t lo = (uint64_t)c * nkmers / p, hi = (uint64_t)(c + 1) * nkmers / p;
                COUNT_RANGE(seq, a.k, lo, hi, INC_PRIV);
            }   /* implicit barrier: all private histograms complete */

            #pragma omp single
            t_count_end = wall_time();

            /* merge: each thread sums a contiguous range of bins over all copies */
            #pragma omp for schedule(static)
            for (uint64_t i = 0; i < nbins; i++) {
                count_t s = 0;
                for (int q = 0; q < p; q++) s += priv[q][i];
                hist[i] = s;
            }
        }
        t_merge = wall_time() - t_count_end;
        for (int q = 0; q < p; q++) free(priv[q]);
        free(priv);
    } else {
        /* ---- false-sharing demo (k = 1): per-thread counters in ONE shared array ---- */
        const int stride = !strcmp(ver, "fspad") ? FS_STRIDE_PADDED : FS_STRIDE_UNPADDED;
        uint64_t *cnt = aligned_alloc(128, 128 * (size_t)((p * stride * 8 + 127) / 128));
        memset(cnt, 0, (size_t)p * stride * 8);
        #pragma omp parallel for schedule(static)
        for (int c = 0; c < p; c++) {
            const uint64_t lo = (uint64_t)c * nkmers / p, hi = (uint64_t)(c + 1) * nkmers / p;
            /* volatile: every increment is a real load+store to the shared array,
             * as it would be if other threads could read these counters        */
            volatile uint64_t *my = cnt + (size_t)c * stride;
#define INC_FS(x) (my[(x)]++)
            COUNT_RANGE(seq, 1, lo, hi, INC_FS);
        }
        for (int c = 0; c < p; c++)
            for (int b = 0; b < 4; b++) hist[b] += (count_t)cnt[(size_t)c * stride + b];
        free(cnt);
    }
    const double t1 = wall_time();

    const uint64_t total = hist_total(hist, nbins);
    const uint64_t cks = hist_checksum(hist, nbins);
    const char *dist = a.skewed ? "skewed" : "uniform";

    printf("version=%s  threads=%d  k=%d  L=%" PRIu64 "  dist=%s  bins=%" PRIu64 " (%.1f MiB)\n",
           ver, p, a.k, a.L, dist, nbins, nbins * sizeof(count_t) / 1048576.0);
    printf("generation : %.3f s (not timed)\n", tg);
    printf("total      : %" PRIu64 "  (expected %" PRIu64 ") %s",
           total, nkmers, total == nkmers ? "OK\n" : "WRONG");
    if (total != nkmers) printf("  -> %" PRIu64 " increments lost\n", nkmers - total);
    printf("checksum   : %016" PRIx64 "\n", cks);
    if (extra_bytes)
        printf("extra mem  : %.1f MiB of private histograms (%d x %.1f MiB)\n",
               extra_bytes / 1048576.0, p, nbins * sizeof(count_t) / 1048576.0);
    printf("time       : %.4f s  (%.1f M k-mers/s)", t1 - t0, nkmers / (t1 - t0) / 1e6);
    if (t_merge > 0) printf("   [merge %.4f s]", t_merge);
    printf("\n");

    long bad_bins = -1;
    if (verify) {
        count_t *ref = calloc(nbins, sizeof(count_t));
#define INC_REF(x) (ref[(x)]++)
        COUNT_RANGE(seq, a.k, 0, nkmers, INC_REF);
        bad_bins = 0;
        for (uint64_t i = 0; i < nbins; i++) bad_bins += (hist[i] != ref[i]);
        printf("verify     : %ld of %" PRIu64 " bins differ from the serial histogram -> %s\n",
               bad_bins, nbins, bad_bins == 0 ? "PASS" : "FAIL");
        free(ref);
    }

    printf("RESULT version=%s p=%d k=%d L=%" PRIu64 " dist=%s total=%" PRIu64
           " checksum=%016" PRIx64 " badbins=%ld extra_mib=%.1f merge=%.6f time=%.6f\n",
           ver, p, a.k, a.L, dist, total, cks, bad_bins,
           extra_bytes / 1048576.0, t_merge, t1 - t0);
    free(hist);
    free(seq);
    return 0;
}

/*
 * parallel.c - OpenMP Monte Carlo pricer for a European call (Case Study 3).
 *
 * Usage:  OMP_NUM_THREADS=p ./parallel [M] [seed] [mode]
 *         defaults: M = 1e8, seed = 42, mode = jump
 *
 * Modes:
 *   jump     (correct)  every thread starts from the same seeded generator and
 *                       calls rng_jump() tid times -> non-overlapping streams.
 *   shared   (bad #1)   ONE generator shared by all threads, protected by
 *                       omp critical -> correct statistics but serialised.
 *   sameseed (bad #2)   every thread seeds its own generator with the SAME seed
 *                       -> all threads draw identical numbers; the reported
 *                       standard error is too small by a factor of sqrt(p).
 *
 * Decomposition: thread t owns samples [t*M/p, (t+1)*M/p) - a fixed, static
 * block. Each thread writes its two partial sums into its own slot of an
 * array, and the slots are added IN THREAD ORDER after the parallel region,
 * so the same (seed, p) always gives a bit-identical answer.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <omp.h>
#include "mc_common.h"

#define MAX_THREADS 256

/* One slot per thread, padded to a 64-byte cache line so that the final
 * write of one thread never shares a line with a neighbour's slot.       */
typedef struct {
    double sum, sumsq, time;
    char pad[64 - 3 * sizeof(double)];
} slot_t;

/* Bad mode #1: every pair of normals comes from one shared generator
 * inside a critical section, so only one thread can draw at a time.     */
static void mc_block_shared(const option_t *o, rng_t *shared, uint64_t n,
                            double *sum_out, double *sumsq_out)
{
    const double drift = (o->r - 0.5 * o->sigma * o->sigma) * o->T;
    const double vol   = o->sigma * sqrt(o->T);
    double sum = 0.0, sumsq = 0.0;

    for (uint64_t i = 0; i < n; i += 2) {
        double z[2];
        #pragma omp critical(shared_rng)
        box_muller(shared, &z[0], &z[1]);
        const int m = (n - i >= 2) ? 2 : 1;
        for (int k = 0; k < m; k++) {
            const double ST = o->S0 * exp(drift + vol * z[k]);
            const double payoff = ST > o->K ? ST - o->K : 0.0;
            sum   += payoff;
            sumsq += payoff * payoff;
        }
    }
    *sum_out = sum;
    *sumsq_out = sumsq;
}

int main(int argc, char **argv)
{
    const uint64_t M    = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    const uint64_t seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 42ULL;
    const char    *mode = (argc > 3) ? argv[3] : "jump";
    const option_t opt  = DEFAULT_OPTION;

    if (strcmp(mode, "jump") && strcmp(mode, "shared") && strcmp(mode, "sameseed")) {
        fprintf(stderr, "mode must be jump | shared | sameseed\n");
        return 1;
    }
    const int p = omp_get_max_threads();
    if (p > MAX_THREADS) { fprintf(stderr, "too many threads\n"); return 1; }
    if (M < 2 * (uint64_t)p) { fprintf(stderr, "M too small for %d threads\n", p); return 1; }

    static slot_t slot[MAX_THREADS];
    rng_t shared_rng;
    rng_seed(&shared_rng, seed);

    const double t0 = omp_get_wtime();
    #pragma omp parallel num_threads(p)
    {
        const int tid = omp_get_thread_num();
        const uint64_t lo = (uint64_t)tid * M / p;
        const uint64_t hi = (uint64_t)(tid + 1) * M / p;
        const double ts = omp_get_wtime();
        double s, sq;

        if (strcmp(mode, "shared") == 0) {
            mc_block_shared(&opt, &shared_rng, hi - lo, &s, &sq);
        } else {
            rng_t g;
            rng_seed(&g, seed);
            if (strcmp(mode, "jump") == 0)
                for (int j = 0; j < tid; j++) rng_jump(&g);
            /* sameseed: no jump -> every thread replays the same stream */
            mc_block(&opt, &g, hi - lo, &s, &sq);
        }
        slot[tid].sum   = s;
        slot[tid].sumsq = sq;
        slot[tid].time  = omp_get_wtime() - ts;
    }
    const double t1 = omp_get_wtime();

    /* deterministic reduction: always thread 0, 1, 2, ... */
    double sum = 0.0, sumsq = 0.0, tmax = 0.0, tmean = 0.0;
    for (int t = 0; t < p; t++) {
        sum   += slot[t].sum;
        sumsq += slot[t].sumsq;
        tmean += slot[t].time / p;
        if (slot[t].time > tmax) tmax = slot[t].time;
    }

    double price, se;
    mc_finish(&opt, M, sum, sumsq, &price, &se);
    const double bs = black_scholes_call(&opt);

    printf("Mode       : %s   threads=%d\n", mode, p);
    printf("Samples M  : %" PRIu64 "   seed=%" PRIu64 "\n", M, seed);
    printf("MC price   : %.6f\n", price);
    printf("Std. error : %.6f\n", se);
    printf("BS price   : %.6f\n", bs);
    printf("|MC - BS|  : %.6f  (%.2f standard errors)\n",
           fabs(price - bs), fabs(price - bs) / se);
    printf("Time       : %.4f s  (%.2f M samples/s)\n",
           t1 - t0, (double)M / (t1 - t0) / 1e6);
    printf("Per-thread : max %.4f s, mean %.4f s, imbalance %.3f\n",
           tmax, tmean, tmax / tmean);

    printf("RESULT version=parallel mode=%s p=%d M=%" PRIu64 " seed=%" PRIu64
           " price=%.10f se=%.10f time=%.6f\n", mode, p, M, seed, price, se, t1 - t0);
    return 0;
}

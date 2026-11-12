/*
 * serial.c - serial Monte Carlo pricer for a European call (Case Study 3).
 *
 * Usage:  ./serial [M] [seed]        defaults: M = 1e8, seed = 42
 *
 * One xoshiro256** generator, seeded once, produces all M samples.
 * Only the sampling loop is timed.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "mc_common.h"

int main(int argc, char **argv)
{
    const uint64_t M    = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    const uint64_t seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 42ULL;
    const option_t opt  = DEFAULT_OPTION;

    if (M < 2) { fprintf(stderr, "M must be >= 2\n"); return 1; }

    rng_t g;
    rng_seed(&g, seed);

    double sum, sumsq;
    const double t0 = wall_time();
    mc_block(&opt, &g, M, &sum, &sumsq);
    const double t1 = wall_time();

    double price, se;
    mc_finish(&opt, M, sum, sumsq, &price, &se);
    const double bs = black_scholes_call(&opt);

    printf("Parameters : S0=%.2f K=%.2f r=%.3f sigma=%.3f T=%.2f\n",
           opt.S0, opt.K, opt.r, opt.sigma, opt.T);
    printf("Samples M  : %" PRIu64 "   seed=%" PRIu64 "\n", M, seed);
    printf("MC price   : %.6f\n", price);
    printf("Std. error : %.6f\n", se);
    printf("BS price   : %.6f\n", bs);
    printf("|MC - BS|  : %.6f  (%.2f standard errors)\n",
           fabs(price - bs), fabs(price - bs) / se);
    printf("Time       : %.4f s  (%.2f M samples/s)\n",
           t1 - t0, (double)M / (t1 - t0) / 1e6);

    /* one machine-readable line for the timing scripts */
    printf("RESULT version=serial p=1 M=%" PRIu64 " seed=%" PRIu64
           " price=%.10f se=%.10f time=%.6f\n", M, seed, price, se, t1 - t0);
    return 0;
}

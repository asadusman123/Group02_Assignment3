/*
 * assoc.c - isolates floating-point non-associativity (Case Study 3, Q4).
 *
 * Usage: ./assoc [M] [seed]
 *
 * Draws ONE stream of M samples (exactly the serial program's samples) and
 * accumulates the payoff sum in several groupings at the same time:
 *   - one running accumulator (what serial.c does)
 *   - p contiguous chunks, each with its own accumulator, combined 0..p-1
 *   - the same chunks combined in reverse order p-1..0
 * The samples are identical in every case, so any difference in the price
 * comes only from the order of floating-point additions.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "mc_common.h"

#define NP 3
static const int PS[NP] = { 2, 4, 8 };

int main(int argc, char **argv)
{
    const uint64_t M    = (argc > 1) ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    const uint64_t seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 42ULL;
    const option_t o = DEFAULT_OPTION;
    const double drift = (o.r - 0.5 * o.sigma * o.sigma) * o.T;
    const double vol   = o.sigma * sqrt(o.T);
    const double disc  = exp(-o.r * o.T);

    rng_t g;
    rng_seed(&g, seed);

    double single = 0.0;
    double chunk[NP][8] = { { 0 } };

    for (uint64_t i = 0; i < M; i += 2) {
        double z[2];
        box_muller(&g, &z[0], &z[1]);
        const int m = (M - i >= 2) ? 2 : 1;
        for (int k = 0; k < m; k++) {
            const uint64_t idx = i + k;
            const double ST = o.S0 * exp(drift + vol * z[k]);
            const double payoff = ST > o.K ? ST - o.K : 0.0;
            single += payoff;
            for (int j = 0; j < NP; j++)
                chunk[j][idx * PS[j] / M] += payoff;
        }
    }

    const double ref = disc * single / (double)M;
    printf("single accumulator     : price = %.17f\n", ref);
    for (int j = 0; j < NP; j++) {
        double fwd = 0.0, rev = 0.0;
        for (int t = 0; t < PS[j]; t++)          fwd += chunk[j][t];
        for (int t = PS[j] - 1; t >= 0; t--)     rev += chunk[j][t];
        const double pf = disc * fwd / (double)M, pr = disc * rev / (double)M;
        printf("%d chunks, forward      : price = %.17f   diff = %+.3e\n", PS[j], pf, pf - ref);
        printf("%d chunks, reverse      : price = %.17f   diff = %+.3e\n", PS[j], pr, pr - ref);
    }
    return 0;
}

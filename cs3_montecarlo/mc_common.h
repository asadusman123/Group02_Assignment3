/*
 * mc_common.h - shared code for the serial and parallel Monte Carlo pricers.
 *
 * Contents:
 *   - xoshiro256** uniform generator (Blackman & Vigna, 2018) + its jump()
 *   - splitmix64, used only to expand one 64-bit seed into 256 bits of state
 *   - Box-Muller transform (uniform -> standard normal)
 *   - mc_block(): prices n samples from one generator (used by BOTH versions)
 *   - Black-Scholes closed form, wall-clock timer
 */
#ifndef MC_COMMON_H
#define MC_COMMON_H

#include <stdint.h>
#include <math.h>
#include <time.h>

/* M_PI / M_SQRT2 are not part of strict ISO C, so define our own. */
#define MC_PI    3.14159265358979323846
#define MC_SQRT2 1.41421356237309504880

/* ---------------- option parameters ---------------- */
typedef struct {
    double S0;    /* spot price            */
    double K;     /* strike                */
    double r;     /* risk-free rate        */
    double sigma; /* volatility            */
    double T;     /* maturity in years     */
} option_t;

static const option_t DEFAULT_OPTION = { 100.0, 100.0, 0.05, 0.20, 1.0 };

/* ---------------- splitmix64 (seed expander) ---------------- */
static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* ---------------- xoshiro256** ---------------- */
typedef struct { uint64_t s[4]; } rng_t;

static inline uint64_t rotl64(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

static inline void rng_seed(rng_t *g, uint64_t seed)
{
    uint64_t x = seed;
    for (int i = 0; i < 4; i++) g->s[i] = splitmix64(&x);
}

static inline uint64_t rng_next(rng_t *g)
{
    uint64_t *s = g->s;
    const uint64_t result = rotl64(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);
    return result;
}

/* Advance the generator by 2^128 steps. Calling it i times gives worker i a
 * stream that cannot overlap any other worker's (each has 2^128 numbers). */
static inline void rng_jump(rng_t *g)
{
    static const uint64_t JUMP[] = { 0x180ec6d33cfd0abaULL, 0xd5a61266f0c9392cULL,
                                     0xa9582618e03fc9aaULL, 0x39abdc4529b1661cULL };
    uint64_t s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (int i = 0; i < 4; i++)
        for (int b = 0; b < 64; b++) {
            if (JUMP[i] & (1ULL << b)) {
                s0 ^= g->s[0]; s1 ^= g->s[1]; s2 ^= g->s[2]; s3 ^= g->s[3];
            }
            rng_next(g);
        }
    g->s[0] = s0; g->s[1] = s1; g->s[2] = s2; g->s[3] = s3;
}

/* Uniform in (0,1]: top 53 bits, +1 so the result is never 0 (log(0) = -inf). */
static inline double rng_uniform(rng_t *g)
{
    return (double)((rng_next(g) >> 11) + 1) * 0x1.0p-53;
}

/* ---------------- Box-Muller: two uniforms -> two N(0,1) ---------------- */
static inline void box_muller(rng_t *g, double *z0, double *z1)
{
    const double u1 = rng_uniform(g);
    const double u2 = rng_uniform(g);
    const double rad = sqrt(-2.0 * log(u1));
    const double ang = 2.0 * MC_PI * u2;
    *z0 = rad * cos(ang);
    *z1 = rad * sin(ang);
}

/* ---------------- the Monte Carlo kernel ----------------
 * Simulates n samples with generator g and returns (via pointers) the
 * UNDISCOUNTED sums  sum(payoff)  and  sum(payoff^2).
 * Box-Muller yields normals in pairs, so both are used; an odd n just drops
 * the last spare normal.                                                    */
static inline void mc_block(const option_t *o, rng_t *g, uint64_t n,
                            double *sum_out, double *sumsq_out)
{
    const double drift = (o->r - 0.5 * o->sigma * o->sigma) * o->T;
    const double vol   = o->sigma * sqrt(o->T);
    double sum = 0.0, sumsq = 0.0;

    for (uint64_t i = 0; i < n; i += 2) {
        double z[2];
        box_muller(g, &z[0], &z[1]);
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

/* Turn raw sums into the discounted price and its standard error. */
static inline void mc_finish(const option_t *o, uint64_t M, double sum, double sumsq,
                             double *price, double *stderr_out)
{
    const double disc = exp(-o->r * o->T);
    const double mean = sum / (double)M;
    const double var  = (sumsq / (double)M - mean * mean) * (double)M / (double)(M - 1);
    *price = disc * mean;
    *stderr_out = disc * sqrt(var / (double)M);
}

/* ---------------- Black-Scholes closed form ---------------- */
static inline double norm_cdf(double x) { return 0.5 * erfc(-x / MC_SQRT2); }

static inline double black_scholes_call(const option_t *o)
{
    const double sT = o->sigma * sqrt(o->T);
    const double d1 = (log(o->S0 / o->K) + (o->r + 0.5 * o->sigma * o->sigma) * o->T) / sT;
    const double d2 = d1 - sT;
    return o->S0 * norm_cdf(d1) - o->K * exp(-o->r * o->T) * norm_cdf(d2);
}

/* ---------------- wall-clock timer ---------------- */
static inline double wall_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

#endif

/*
 * kmer_common.h - shared code for the serial and parallel k-mer counters.
 *
 *   - deterministic synthetic DNA generator (uniform or skewed base mix)
 *   - 2-bit base encoding  A=0 C=1 G=2 T=3
 *   - count_range(): the rolling-window kernel used by every version
 *   - histogram checksum, timer
 */
#ifndef KMER_COMMON_H
#define KMER_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint32_t count_t;          /* counts <= L - k + 1 < 2^32 for L <= 4e9 */

/* ---------------- deterministic RNG (splitmix64) ---------------- */
static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* ---------------- sequence generation ----------------
 * uniform: each base equally likely (2 random bits per base).
 * skewed : P(A)=0.55 P(C)=0.05 P(G)=0.05 P(T)=0.35, so a few k-mers
 *          (AAAA..., runs of A/T) dominate the histogram.               */
static char *make_sequence(uint64_t L, uint64_t seed, int skewed)
{
    static const char BASES[4] = { 'A', 'C', 'G', 'T' };
    char *s = malloc(L + 1);
    if (!s) { fprintf(stderr, "out of memory for sequence\n"); exit(1); }
    uint64_t x = seed;
    if (!skewed) {
        for (uint64_t i = 0; i < L; i += 32) {
            uint64_t r = splitmix64(&x);
            for (int j = 0; j < 32 && i + j < L; j++, r >>= 2)
                s[i + j] = BASES[r & 3];
        }
    } else {
        for (uint64_t i = 0; i < L; i++) {
            const uint32_t u = (uint32_t)(splitmix64(&x) >> 32) % 100u;
            s[i] = u < 55 ? 'A' : u < 60 ? 'C' : u < 65 ? 'G' : 'T';
        }
    }
    s[L] = '\0';
    return s;
}

/* ASCII -> 2-bit code. Only A, C, G, T occur in our sequences. */
static unsigned char ENC[256];
static inline void init_encoding(void)
{
    memset(ENC, 0, sizeof ENC);
    ENC['A'] = 0; ENC['C'] = 1; ENC['G'] = 2; ENC['T'] = 3;
}

/* ---------------- the kernel ----------------
 * Counts the k-mers whose START position lies in [lo, hi).
 * The k-mer starting at i occupies seq[i .. i+k-1], so the window is primed
 * with the k-1 bases from lo; this may read into the next chunk, which is
 * fine (read-only) and is exactly what makes boundary k-mers count once.
 * INCREMENT(code) is supplied by each version (plain, atomic, private...). */
#define COUNT_RANGE(seq, k, lo, hi, INCREMENT)                          \
    do {                                                                \
        const uint64_t mask_ = ((k) == 32) ? ~0ULL : ((1ULL << (2 * (k))) - 1); \
        uint64_t code_ = 0;                                             \
        for (uint64_t j_ = (lo); j_ + 1 < (lo) + (uint64_t)(k); j_++)             \
            code_ = (code_ << 2) | ENC[(unsigned char)(seq)[j_]];       \
        for (uint64_t i_ = (lo); i_ < (hi); i_++) {                     \
            code_ = ((code_ << 2) | ENC[(unsigned char)(seq)[i_ + (k) - 1]]) & mask_; \
            INCREMENT(code_);                                           \
        }                                                               \
    } while (0)

/* ---------------- verification helpers ---------------- */
static inline uint64_t hist_total(const count_t *h, uint64_t nbins)
{
    uint64_t t = 0;
    for (uint64_t i = 0; i < nbins; i++) t += h[i];
    return t;
}

/* FNV-1a over the counts: equal checksums <=> (almost surely) equal histograms */
static inline uint64_t hist_checksum(const count_t *h, uint64_t nbins)
{
    uint64_t x = 1469598103934665603ULL;
    for (uint64_t i = 0; i < nbins; i++) { x ^= h[i]; x *= 1099511628211ULL; }
    return x;
}

static inline double wall_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

/* ---------------- argument parsing ----------------
 * ./prog [k] [L] [dist] [seed]    dist = uniform | skewed               */
typedef struct { int k; uint64_t L; int skewed; uint64_t seed; } args_t;

static inline args_t parse_args(int argc, char **argv, int first)
{
    args_t a = { 12, 100000000ULL, 0, 2026 };
    if (argc > first)     a.k = atoi(argv[first]);
    if (argc > first + 1) a.L = strtoull(argv[first + 1], NULL, 10);
    if (argc > first + 2) a.skewed = strcmp(argv[first + 2], "skewed") == 0;
    if (argc > first + 3) a.seed = strtoull(argv[first + 3], NULL, 10);
    if (a.k < 1 || a.k > 14) { fprintf(stderr, "k must be in 1..14\n"); exit(1); }
    if (a.L < (uint64_t)a.k) { fprintf(stderr, "L must be >= k\n"); exit(1); }
    return a;
}

#endif

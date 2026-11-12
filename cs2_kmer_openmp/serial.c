/*
 * serial.c - serial k-mer counter (Case Study 2).
 *
 * Usage: ./serial [k] [L] [uniform|skewed] [seed]
 *        defaults: k = 12, L = 1e8, uniform, seed = 2026
 *
 * Slides a k-base window across the sequence; each window is a 2k-bit
 * integer used directly as the histogram index (4^k bins).
 * Timed: counting only (sequence generation and histogram allocation are
 * reported separately).
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include "kmer_common.h"

int main(int argc, char **argv)
{
    const args_t a = parse_args(argc, argv, 1);
    init_encoding();

    double tg = wall_time();
    char *seq = make_sequence(a.L, a.seed, a.skewed);
    tg = wall_time() - tg;

    const uint64_t nbins = 1ULL << (2 * a.k);
    count_t *hist = calloc(nbins, sizeof(count_t));
    if (!hist) { fprintf(stderr, "out of memory\n"); return 1; }

    const uint64_t nkmers = a.L - a.k + 1;
    const double t0 = wall_time();
#define INC_SERIAL(c) (hist[(c)]++)
    COUNT_RANGE(seq, a.k, 0, nkmers, INC_SERIAL);
    const double t1 = wall_time();

    const uint64_t total = hist_total(hist, nbins);
    const uint64_t cks = hist_checksum(hist, nbins);
    uint64_t maxbin = 0;
    for (uint64_t i = 1; i < nbins; i++) if (hist[i] > hist[maxbin]) maxbin = i;

    printf("k=%d  L=%" PRIu64 "  dist=%s  seed=%" PRIu64 "  bins=%" PRIu64 " (%.1f MiB)\n",
           a.k, a.L, a.skewed ? "skewed" : "uniform", a.seed, nbins,
           nbins * sizeof(count_t) / 1048576.0);
    printf("generation : %.3f s (not timed)\n", tg);
    printf("total      : %" PRIu64 "  (expected L-k+1 = %" PRIu64 ") %s\n",
           total, nkmers, total == nkmers ? "OK" : "MISMATCH");
    printf("max bin    : index %" PRIu64 " count %u\n", maxbin, hist[maxbin]);
    printf("checksum   : %016" PRIx64 "\n", cks);
    printf("time       : %.4f s  (%.1f M k-mers/s)\n", t1 - t0, nkmers / (t1 - t0) / 1e6);
    printf("RESULT version=serial p=1 k=%d L=%" PRIu64 " dist=%s total=%" PRIu64
           " checksum=%016" PRIx64 " time=%.6f\n",
           a.k, a.L, a.skewed ? "skewed" : "uniform", total, cks, t1 - t0);
    free(hist);
    free(seq);
    return 0;
}

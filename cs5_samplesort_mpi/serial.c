/*
 * serial.c - single-process baseline for Case Study 5: generate N keys and sort them.
 *
 * Usage: ./serial [N] [uniform|zipf] [qsort|radix] [seed]
 *        defaults: N = 1e8, uniform, qsort, seed = 2026
 *
 * The N keys are generated as ONE shard (shard 0) - same generator and
 * distribution as the parallel version. Timed: the sort only.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <time.h>
#include "ss_common.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }

int main(int argc, char **argv)
{
    const uint64_t N = argc > 1 ? strtoull(argv[1], NULL, 10) : 100000000ULL;
    const int zipf  = argc > 2 && !strcmp(argv[2], "zipf");
    const int radix = argc > 3 && !strcmp(argv[3], "radix");
    const uint64_t seed = argc > 4 ? strtoull(argv[4], NULL, 10) : 2026;

    uint32_t *a = malloc(N * sizeof(uint32_t));
    if (!a) { fprintf(stderr, "out of memory\n"); return 1; }
    double tg = now();
    gen_keys(a, N, zipf, seed, 0);
    tg = now() - tg;
    const uint64_t fp0 = fingerprint(a, N);

    const double t0 = now();
    local_sort(a, N, radix);
    const double t1 = now();

    const int ok = is_sorted_u32(a, N) && fingerprint(a, N) == fp0;
    printf("N=%" PRIu64 " dist=%s sort=%s  (generation %.2f s, not timed)\n", N, zipf ? "zipf" : "uniform",
           radix ? "radix" : "qsort", tg);
    printf("sorted     : %s (order and multiset fingerprint)\n", ok ? "PASS" : "FAIL");
    printf("time       : %.4f s  (%.1f M keys/s)\n", t1 - t0, N / (t1 - t0) / 1e6);
    printf("RESULT version=serial p=1 N=%" PRIu64 " dist=%s sort=%s ov=0 tie=0 time=%.6f t_local=%.6f "
           "t_sample=0 t_part=0 t_a2a=0 t_merge=0 maxcount=%" PRIu64 " ratio=1.000 pass=%d\n",
           N, zipf ? "zipf" : "uniform", radix ? "radix" : "qsort", t1 - t0, t1 - t0, N, ok);
    free(a);
    return ok ? 0 : 1;
}

/*
 * pr_common.h - graph file format, loader and helpers for Case Study 4.
 *
 * Graph file (written by gen.c), all little-endian:
 *   char     magic[8] = "PRCSR001"
 *   uint32_t n                      vertices
 *   uint64_t m                      edges (directed, no self-loops, no duplicates)
 *   uint32_t out_off[n+1], out_dst[m]   CSR of OUT-edges  (used by push)
 *   uint32_t in_off[n+1],  in_src[m]    CSR of IN-edges   (used by pull)
 * Offsets are 32-bit, so m must be < 2^32.
 */
#ifndef PR_COMMON_H
#define PR_COMMON_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define DAMPING 0.85

typedef struct {
    uint32_t n;
    uint64_t m;
    uint32_t *out_off, *out_dst;
    uint32_t *in_off, *in_src;
} graph_t;

static inline double wall_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static inline void read_or_die(void *p, size_t sz, size_t cnt, FILE *f)
{
    if (fread(p, sz, cnt, f) != cnt) { fprintf(stderr, "graph file truncated\n"); exit(1); }
}

static inline graph_t load_graph(const char *path)
{
    graph_t g;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s (run ./gen first)\n", path); exit(1); }
    char magic[8];
    read_or_die(magic, 1, 8, f);
    if (memcmp(magic, "PRCSR001", 8)) { fprintf(stderr, "%s: not a graph file\n", path); exit(1); }
    read_or_die(&g.n, sizeof g.n, 1, f);
    read_or_die(&g.m, sizeof g.m, 1, f);
    g.out_off = malloc((g.n + 1) * sizeof(uint32_t));
    g.out_dst = malloc(g.m * sizeof(uint32_t));
    g.in_off  = malloc((g.n + 1) * sizeof(uint32_t));
    g.in_src  = malloc(g.m * sizeof(uint32_t));
    if (!g.out_off || !g.out_dst || !g.in_off || !g.in_src) { fprintf(stderr, "out of memory\n"); exit(1); }
    read_or_die(g.out_off, sizeof(uint32_t), g.n + 1, f);
    read_or_die(g.out_dst, sizeof(uint32_t), g.m, f);
    read_or_die(g.in_off,  sizeof(uint32_t), g.n + 1, f);
    read_or_die(g.in_src,  sizeof(uint32_t), g.m, f);
    fclose(f);
    return g;
}

static inline uint32_t outdeg(const graph_t *g, uint32_t u) { return g->out_off[u + 1] - g->out_off[u]; }

/* write / compare rank vectors (raw doubles) */
static inline void write_ranks(const char *path, const double *r, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(r, sizeof(double), n, f) != n) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fclose(f);
}

/* returns L1 distance to the reference file (and max abs diff via *maxd); -1 if unreadable */
static inline double compare_ranks(const char *path, const double *r, uint32_t n, double *maxd)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1.0;
    double *ref = malloc(n * sizeof(double));
    if (fread(ref, sizeof(double), n, f) != n) { fclose(f); free(ref); return -1.0; }
    fclose(f);
    double l1 = 0.0, mx = 0.0;
    for (uint32_t v = 0; v < n; v++) {
        const double d = fabs(r[v] - ref[v]);
        l1 += d;
        if (d > mx) mx = d;
    }
    free(ref);
    *maxd = mx;
    return l1;
}

/* indices of the top-k ranks (k <= 10), for a human-readable summary */
static inline void top_k(const double *r, uint32_t n, int k, uint32_t *idx)
{
    for (int i = 0; i < k; i++) idx[i] = UINT32_MAX;
    for (uint32_t v = 0; v < n; v++) {
        for (int i = 0; i < k; i++) {
            if (idx[i] == UINT32_MAX || r[v] > r[idx[i]]) {
                for (int j = k - 1; j > i; j--) idx[j] = idx[j - 1];
                idx[i] = v;
                break;
            }
        }
    }
}

#endif

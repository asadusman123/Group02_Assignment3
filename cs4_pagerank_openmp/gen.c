/*
 * gen.c - synthetic power-law graph generator (R-MAT) for Case Study 4.
 *
 * Usage: ./gen [scale] [edgefactor] [seed] [out.bin] [degrees.csv]
 *        defaults: scale 20 (n = 2^20 = 1,048,576 vertices), edgefactor 16,
 *                  seed 2026, graph_s20.bin, degrees_s20.csv
 *
 * R-MAT (Chakrabarti, Zhan, Faloutsos, SDM 2004) with the Graph500
 * parameters a = 0.57, b = 0.19, c = 0.19, d = 0.05: each edge picks one of
 * the four quadrants of the adjacency matrix recursively, scale times.
 * edgefactor * n directed edges are drawn; self-loops and duplicate edges are
 * removed. Vertex ids are NOT permuted, so high-degree vertices cluster at
 * low ids (this matters for static scheduling - see the report).
 * Writes the graph (both CSR directions) and a degree histogram.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include "pr_common.h"

static inline uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* LSD radix sort of 64-bit keys, 16 bits per pass */
static void radix_sort(uint64_t *a, uint64_t n)
{
    uint64_t *tmp = malloc(n * sizeof(uint64_t));
    uint64_t *cnt = malloc(65536 * sizeof(uint64_t));
    for (int shift = 0; shift < 64; shift += 16) {
        memset(cnt, 0, 65536 * sizeof(uint64_t));
        for (uint64_t i = 0; i < n; i++) cnt[(a[i] >> shift) & 0xFFFF]++;
        uint64_t s = 0;
        for (int b = 0; b < 65536; b++) { uint64_t c = cnt[b]; cnt[b] = s; s += c; }
        for (uint64_t i = 0; i < n; i++) tmp[cnt[(a[i] >> shift) & 0xFFFF]++] = a[i];
        uint64_t *t = a; a = tmp; tmp = t;
    }
    /* 4 passes: the data is back in the original buffer */
    free(tmp);
    free(cnt);
}

int main(int argc, char **argv)
{
    const int scale = argc > 1 ? atoi(argv[1]) : 20;
    const int ef    = argc > 2 ? atoi(argv[2]) : 16;
    uint64_t seed   = argc > 3 ? strtoull(argv[3], NULL, 10) : 2026;
    char defout[64], defcsv[64];
    snprintf(defout, sizeof defout, "graph_s%d.bin", scale);
    snprintf(defcsv, sizeof defcsv, "degrees_s%d.csv", scale);
    const char *out = argc > 4 ? argv[4] : defout;
    const char *csv = argc > 5 ? argv[5] : defcsv;
    if (scale < 4 || scale > 30) { fprintf(stderr, "scale must be 4..30\n"); return 1; }

    const uint32_t n = 1u << scale;
    const uint64_t M = (uint64_t)ef * n;
    const double A = 0.57, B = 0.19, C = 0.19;          /* D = 0.05 */
    double t0 = wall_time();

    uint64_t *e = malloc(M * sizeof(uint64_t));
    if (!e) { fprintf(stderr, "out of memory\n"); return 1; }
    for (uint64_t k = 0; k < M; k++) {
        uint32_t u = 0, v = 0;
        for (int lvl = 0; lvl < scale; lvl++) {
            const double x = (double)(splitmix64(&seed) >> 11) * 0x1.0p-53;
            const int ub = x >= A + B;                       /* lower half (c or d) */
            const int vb = (x >= A && x < A + B) || x >= A + B + C;   /* right half (b or d) */
            u = (u << 1) | (uint32_t)ub;
            v = (v << 1) | (uint32_t)vb;
        }
        e[k] = ((uint64_t)u << 32) | v;
    }
    radix_sort(e, M);
    uint64_t m = 0;                                     /* drop self-loops and duplicates */
    for (uint64_t k = 0; k < M; k++) {
        const uint32_t u = (uint32_t)(e[k] >> 32), v = (uint32_t)e[k];
        if (u == v) continue;
        if (m > 0 && e[m - 1] == e[k]) continue;
        e[m++] = e[k];
    }
    if (m >= UINT32_MAX) { fprintf(stderr, "too many edges for 32-bit offsets\n"); return 1; }

    /* out-CSR: edges are already sorted by (u, v) */
    uint32_t *out_off = calloc(n + 1, sizeof(uint32_t)), *out_dst = malloc(m * sizeof(uint32_t));
    uint32_t *in_off  = calloc(n + 1, sizeof(uint32_t)), *in_src  = malloc(m * sizeof(uint32_t));
    for (uint64_t k = 0; k < m; k++) { out_off[(e[k] >> 32) + 1]++; in_off[(uint32_t)e[k] + 1]++; }
    for (uint32_t v = 0; v < n; v++) { out_off[v + 1] += out_off[v]; in_off[v + 1] += in_off[v]; }
    uint32_t *pos = malloc(n * sizeof(uint32_t));
    memcpy(pos, in_off, n * sizeof(uint32_t));
    for (uint64_t k = 0; k < m; k++) {
        out_dst[k] = (uint32_t)e[k];
        in_src[pos[(uint32_t)e[k]]++] = (uint32_t)(e[k] >> 32);   /* in-lists sorted by source */
    }
    free(pos);
    free(e);

    FILE *f = fopen(out, "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", out); return 1; }
    fwrite("PRCSR001", 1, 8, f);
    fwrite(&n, sizeof n, 1, f);
    fwrite(&m, sizeof m, 1, f);
    fwrite(out_off, sizeof(uint32_t), n + 1, f);
    fwrite(out_dst, sizeof(uint32_t), m, f);
    fwrite(in_off, sizeof(uint32_t), n + 1, f);
    fwrite(in_src, sizeof(uint32_t), m, f);
    fclose(f);

    /* degree statistics + histogram (for the log-log plot) */
    uint32_t maxin = 0, maxout = 0, dangling = 0, isolated = 0;
    for (uint32_t v = 0; v < n; v++) {
        const uint32_t di = in_off[v + 1] - in_off[v], dout = out_off[v + 1] - out_off[v];
        if (di > maxin) maxin = di;
        if (dout > maxout) maxout = dout;
        dangling += (dout == 0);
        isolated += (dout == 0 && di == 0);
    }
    uint64_t *hin = calloc(maxin + 1, sizeof(uint64_t)), *hout = calloc(maxout + 1, sizeof(uint64_t));
    for (uint32_t v = 0; v < n; v++) { hin[in_off[v + 1] - in_off[v]]++; hout[out_off[v + 1] - out_off[v]]++; }
    FILE *c = fopen(csv, "w");
    fprintf(c, "degree,in_count,out_count\n");
    const uint32_t mx = maxin > maxout ? maxin : maxout;
    for (uint32_t d = 0; d <= mx; d++) {
        const uint64_t a = d <= maxin ? hin[d] : 0, b = d <= maxout ? hout[d] : 0;
        if (a || b) fprintf(c, "%u,%" PRIu64 ",%" PRIu64 "\n", d, a, b);
    }
    fclose(c);

    printf("R-MAT scale=%d edgefactor=%d seed=%s -> %s\n", scale, ef, argc > 3 ? argv[3] : "2026", out);
    printf("vertices n = %u, edges drawn = %" PRIu64 ", kept m = %" PRIu64 " (after removing self-loops/duplicates)\n", n, M, m);
    printf("max in-degree %u, max out-degree %u, average degree %.2f\n", maxin, maxout, (double)m / n);
    printf("dangling vertices (out-degree 0): %u (%.1f%%), isolated: %u\n", dangling, 100.0 * dangling / n, isolated);
    printf("file size %.1f MiB, generation %.2f s\n",
           (16.0 + 8.0 * (n + 1) + 8.0 * m) / 1048576.0, wall_time() - t0);
    printf("RESULT gen n=%u m=%" PRIu64 " maxin=%u maxout=%u dangling=%u isolated=%u\n", n, m, maxin, maxout, dangling, isolated);
    return 0;
}

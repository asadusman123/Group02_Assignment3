/*
 * serial.c - serial pull-style PageRank on a CSR graph (Case Study 4).
 *
 * Usage: ./serial [graph.bin] [eps] [maxiter] [ranks_out.bin]
 *        defaults: graph_s20.bin, eps = 1e-9, maxiter = 200, ranks_serial.bin
 *
 *   r_v(t+1) = (1-d)/n + d * ( sum_{u->v} r_u / outdeg(u)  +  D(t)/n )
 * d = 0.85. D(t) = total rank of DANGLING vertices (out-degree 0); their rank
 * is spread uniformly over all n vertices, so the ranks always sum to 1.
 * Iterate until ||r(t+1) - r(t)||_1 < eps. Timed: the iterations only.
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include "pr_common.h"

int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "graph_s20.bin";
    const double eps  = argc > 2 ? atof(argv[2]) : 1e-9;
    const int maxiter = argc > 3 ? atoi(argv[3]) : 200;
    const char *rout  = argc > 4 ? argv[4] : "ranks_serial.bin";

    const graph_t g = load_graph(gpath);
    const uint32_t n = g.n;
    double *r = malloc(n * sizeof(double)), *rn = malloc(n * sizeof(double));
    double *contrib = malloc(n * sizeof(double));
    for (uint32_t v = 0; v < n; v++) r[v] = 1.0 / n;

    int it = 0;
    double diff = 1.0;
    const double t0 = wall_time();
    while (it < maxiter && diff >= eps) {
        double D = 0.0;
        for (uint32_t u = 0; u < n; u++) {
            const uint32_t od = outdeg(&g, u);
            if (od) contrib[u] = r[u] / od;
            else  { contrib[u] = 0.0; D += r[u]; }
        }
        const double base = (1.0 - DAMPING) / n + DAMPING * D / n;
        diff = 0.0;
        for (uint32_t v = 0; v < n; v++) {
            double s = 0.0;
            for (uint32_t k = g.in_off[v]; k < g.in_off[v + 1]; k++) s += contrib[g.in_src[k]];
            rn[v] = base + DAMPING * s;
            diff += fabs(rn[v] - r[v]);
        }
        double *t = r; r = rn; rn = t;
        it++;
    }
    const double t1 = wall_time();

    double sum = 0.0;
    for (uint32_t v = 0; v < n; v++) sum += r[v];
    uint32_t top[5];
    top_k(r, n, 5, top);
    write_ranks(rout, r, n);

    printf("graph %s: n=%u m=%" PRIu64 "\n", gpath, n, g.m);
    printf("iterations : %d  (final L1 change %.3e, eps %.1e)\n", it, diff, eps);
    printf("rank sum   : %.15f\n", sum);
    printf("top 5      :");
    for (int i = 0; i < 5; i++) printf(" %u (%.6e)", top[i], r[top[i]]);
    printf("\ntime       : %.4f s  (%.4f s/iter, %.1f M edges/s)\n", t1 - t0, (t1 - t0) / it,
           (double)g.m * it / (t1 - t0) / 1e6);
    printf("RESULT version=serial p=1 sched=none iters=%d time=%.6f per_iter=%.6f medges=%.2f imb=1.000 eshare=1.000 l1=0 pass=1\n",
           it, t1 - t0, (t1 - t0) / it, (double)g.m * it / (t1 - t0) / 1e6);
    return 0;
}

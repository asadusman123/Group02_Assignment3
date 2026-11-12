/*
 * parallel.c - OpenMP PageRank, pull and push styles (Case Study 4).
 *
 * Usage: OMP_NUM_THREADS=p OMP_SCHEDULE=<sched> ./parallel VERSION [graph.bin] [eps] [maxiter] [ref_ranks.bin]
 *        defaults: graph_s20.bin, eps = 1e-9, maxiter = 200, ranks_serial.bin
 *        <sched> e.g. "static", "static,64", "dynamic,256", "guided"  (schedule(runtime))
 *
 * VERSION
 *   pull    each thread owns a set of DESTINATION vertices v and sums the
 *           contributions of v's in-neighbours (in-CSR). Only reads are shared;
 *           every write goes to the thread's own rn[v]. No synchronisation.
 *   push    each thread owns a set of SOURCE vertices u and adds r_u/outdeg(u)
 *           to next[v] for every out-neighbour v (out-CSR) with omp atomic.
 *   pushna  push WITHOUT atomics - a data race, only meaningful at p = 1 where it
 *           isolates the cost of the atomic instruction itself.
 *
 * The vertex loop that does the edge work uses schedule(runtime). Per-thread
 * time spent in it (nowait, so idle time is not counted) and the number of
 * edges each thread processed are accumulated over all iterations:
 *   imbalance R = slowest thread time / mean thread time
 *   eshare      = max edges per thread / mean edges per thread
 * The final ranks are compared with the serial ranks (L1 distance).
 */
#define _POSIX_C_SOURCE 200809L
#include <inttypes.h>
#include <omp.h>
#include "pr_common.h"

#define MAXT 256
#define TOL  1e-9          /* PASS if || r_parallel - r_serial ||_1 < TOL */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s pull|push|pushna [graph.bin] [eps] [maxiter] [ref_ranks.bin]\n", argv[0]);
        return 1;
    }
    const char *ver   = argv[1];
    const int is_pull = !strcmp(ver, "pull"), is_push = !strcmp(ver, "push"), is_na = !strcmp(ver, "pushna");
    if (!is_pull && !is_push && !is_na) { fprintf(stderr, "VERSION must be pull, push or pushna\n"); return 1; }
    const char *gpath = argc > 2 ? argv[2] : "graph_s20.bin";
    const double eps  = argc > 3 ? atof(argv[3]) : 1e-9;
    const int maxiter = argc > 4 ? atoi(argv[4]) : 200;
    const char *ref   = argc > 5 ? argv[5] : "ranks_serial.bin";

    const graph_t g = load_graph(gpath);
    const uint32_t n = g.n;
    double *r = malloc(n * sizeof(double)), *rn = malloc(n * sizeof(double));
    double *contrib = malloc(n * sizeof(double));     /* pull: r_u/outdeg(u) */
    double *next = malloc(n * sizeof(double));        /* push: accumulated in-flow */
    const int p = omp_get_max_threads();
    if (p > MAXT) { fprintf(stderr, "too many threads\n"); return 1; }

    #pragma omp parallel for schedule(static)
    for (uint32_t v = 0; v < n; v++) { r[v] = 1.0 / n; rn[v] = contrib[v] = next[v] = 0.0; }

    double tthr[MAXT] = { 0 };
    uint64_t ethr[MAXT] = { 0 };
    int it = 0;
    double diff = 1.0;
    const double t0 = omp_get_wtime();

    while (it < maxiter && diff >= eps) {
        double D = 0.0, df = 0.0;
        #pragma omp parallel
        {
            const int t = omp_get_thread_num();

            /* phase 1: per-source contribution, dangling mass D, clear next[] */
            #pragma omp for schedule(static) reduction(+:D)
            for (uint32_t u = 0; u < n; u++) {
                const uint32_t od = outdeg(&g, u);
                if (od) contrib[u] = r[u] / od;
                else  { contrib[u] = 0.0; D += r[u]; }
                next[u] = 0.0;
            }   /* implicit barrier: D and contrib complete */
            const double base = (1.0 - DAMPING) / n + DAMPING * D / n;

            double ts = omp_get_wtime();
            uint64_t edges = 0;
            if (is_pull) {
                /* phase 2 (pull): v gathers from its in-neighbours; writes only rn[v] */
                #pragma omp for schedule(runtime) reduction(+:df) nowait
                for (uint32_t v = 0; v < n; v++) {
                    double s = 0.0;
                    const uint32_t a = g.in_off[v], b = g.in_off[v + 1];
                    for (uint32_t k = a; k < b; k++) s += contrib[g.in_src[k]];
                    edges += b - a;
                    rn[v] = base + DAMPING * s;
                    df += fabs(rn[v] - r[v]);
                }
                tthr[t] += omp_get_wtime() - ts;
                ethr[t] += edges;
            } else {
                /* phase 2 (push): u scatters to its out-neighbours */
                #pragma omp for schedule(runtime) nowait
                for (uint32_t u = 0; u < n; u++) {
                    const uint32_t a = g.out_off[u], b = g.out_off[u + 1];
                    const double c = contrib[u];
                    if (is_push) {
                        for (uint32_t k = a; k < b; k++) {
                            #pragma omp atomic
                            next[g.out_dst[k]] += c;
                        }
                    } else {
                        for (uint32_t k = a; k < b; k++) next[g.out_dst[k]] += c;
                    }
                    edges += b - a;
                }
                tthr[t] += omp_get_wtime() - ts;
                ethr[t] += edges;
                #pragma omp barrier
                /* phase 3 (push): apply damping, teleport and dangling share */
                #pragma omp for schedule(static) reduction(+:df)
                for (uint32_t v = 0; v < n; v++) {
                    rn[v] = base + DAMPING * next[v];
                    df += fabs(rn[v] - r[v]);
                }
            }
        }
        diff = df;
        double *tmp = r; r = rn; rn = tmp;
        it++;
    }
    const double t1 = omp_get_wtime();

    /* load-balance statistics */
    double tmax = 0, tmean = 0;
    uint64_t emax = 0;
    for (int t = 0; t < p; t++) {
        tmean += tthr[t] / p;
        if (tthr[t] > tmax) tmax = tthr[t];
        if (ethr[t] > emax) emax = ethr[t];
    }
    const double R = tmean > 0 ? tmax / tmean : 1.0;
    const double eshare = (double)emax / ((double)g.m * it / p);

    double maxd = 0;
    const double l1 = compare_ranks(ref, r, n, &maxd);
    const int pass = l1 >= 0 && l1 < TOL;

    /* schedule actually used by schedule(runtime): taken from OMP_SCHEDULE */
    char sched[32];
    const char *env = getenv("OMP_SCHEDULE");
    if (env) {
        snprintf(sched, sizeof sched, "%s", env);
        for (char *c = sched; *c; c++) if (*c == ' ') *c = '_';
    } else {
        omp_sched_t kind; int chunk;
        omp_get_schedule(&kind, &chunk);
        snprintf(sched, sizeof sched, "default(kind=%d,chunk=%d)", (int)kind, chunk);
    }

    uint32_t top[5];
    top_k(r, n, 5, top);
    printf("version=%s  threads=%d  schedule=%s  graph %s: n=%u m=%" PRIu64 "\n", ver, p, sched, gpath, n, g.m);
    printf("iterations : %d  (final L1 change %.3e, eps %.1e)\n", it, diff, eps);
    printf("top 5      :");
    for (int i = 0; i < 5; i++) printf(" %u (%.6e)", top[i], r[top[i]]);
    printf("\ntime       : %.4f s  (%.4f s/iter, %.1f M edges/s)\n", t1 - t0, (t1 - t0) / it,
           (double)g.m * it / (t1 - t0) / 1e6);
    printf("balance    : edge loop thread time max/mean R = %.3f, edges max/mean = %.3f\n", R, eshare);
    if (l1 >= 0)
        printf("verify     : L1 distance to %s = %.3e (max abs diff %.3e), tolerance %.0e -> %s\n",
               ref, l1, maxd, TOL, pass ? "PASS" : "FAIL");
    else
        printf("verify     : reference %s not found (run ./serial first with the same eps)\n", ref);
    printf("RESULT version=%s p=%d sched=%s iters=%d time=%.6f per_iter=%.6f medges=%.2f imb=%.3f eshare=%.3f l1=%.3e pass=%d\n",
           ver, p, sched, it, t1 - t0, (t1 - t0) / it, (double)g.m * it / (t1 - t0) / 1e6, R, eshare, l1, pass);
    return 0;
}

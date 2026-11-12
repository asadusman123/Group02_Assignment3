/*
 * serial.c - serial 2-D heat diffusion (Case Study 1).
 *
 * Usage: ./serial [N] [T] [K] [dumpfile]      defaults: N = 1024, T = 500, K = 50
 *
 * Two N x N grids (current, next) swapped every step. Every K steps the
 * maximum absolute change over the step is computed (convergence metric).
 * If dumpfile is given, the final grid is written as N*N raw doubles
 * (row-major) so the MPI version's output can be compared byte for byte.
 * Timed: the T time steps only.
 */
#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include "heat_common.h"

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + 1e-9 * t.tv_nsec; }

int main(int argc, char **argv)
{
    const heat_args_t a = heat_parse(argc, argv);
    const long N = a.N;
    const char *dump = argc > 4 ? argv[4] : NULL;

    double *u = malloc(N * N * sizeof(double));
    double *v = malloc(N * N * sizeof(double));
    if (!u || !v) { fprintf(stderr, "out of memory\n"); return 1; }
    for (long i = 0; i < N; i++)
        for (long j = 0; j < N; j++)
            u[i * N + j] = v[i * N + j] = initial_value(i, j, N);

    double maxdiff = 0.0;
    int checks = 0;
    const double t0 = now();
    for (int t = 1; t <= a.T; t++) {
        if (t % a.K == 0) {
            double md = 0.0;
            for (long i = 1; i < N - 1; i++)
                for (long j = 1; j < N - 1; j++) {
                    const long c = i * N + j;
                    v[c] = STENCIL(u[c], u[c - N], u[c + N], u[c - 1], u[c + 1]);
                    const double d = fabs(v[c] - u[c]);
                    if (d > md) md = d;
                }
            maxdiff = md;
            checks++;
        } else {
            for (long i = 1; i < N - 1; i++)
                for (long j = 1; j < N - 1; j++) {
                    const long c = i * N + j;
                    v[c] = STENCIL(u[c], u[c - N], u[c + N], u[c - 1], u[c + 1]);
                }
        }
        double *tmp = u; u = v; v = tmp;
    }
    const double t1 = now();

    double sum = 0.0;
    for (long c = 0; c < N * N; c++) sum += u[c];

    printf("N=%ld T=%d K=%d  alpha=%.2f\n", N, a.T, a.K, ALPHA);
    printf("checksum   : %.10f  (sum of all cells)\n", sum);
    printf("centre     : %.10f\n", u[(N / 2) * N + N / 2]);
    printf("max change : %.6e at the last check (%d checks)\n", maxdiff, checks);
    printf("time       : %.4f s  (%.1f M cell-updates/s)\n", t1 - t0,
           (double)(N - 2) * (N - 2) * a.T / (t1 - t0) / 1e6);
    printf("RESULT version=serial p=1 dims=1x1 comm=none N=%ld T=%d K=%d checksum=%.10f maxdiff=%.6e "
           "time=%.6f comp=%.6f comm_t=0 reduce=0\n", N, a.T, a.K, sum, maxdiff, t1 - t0, t1 - t0);

    if (dump) {
        FILE *f = fopen(dump, "wb");
        if (!f || fwrite(u, sizeof(double), N * N, f) != (size_t)(N * N)) { fprintf(stderr, "dump failed\n"); return 1; }
        fclose(f);
    }
    free(u); free(v);
    return 0;
}

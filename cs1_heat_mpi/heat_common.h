/*
 * heat_common.h - shared definitions for the serial and MPI heat solvers.
 *
 * Grid: N x N cells, row-major, global indices (i, j) in [0, N).
 * The outermost ring (i or j equal to 0 or N-1) is a FIXED boundary:
 *     top (i = 0) = 100, left (j = 0) = 75, right (j = N-1) = 50, bottom (i = N-1) = 0
 * (corners take the top/bottom value). All interior cells start at 0.
 * Interior cells are updated with the explicit 5-point stencil
 *     u'[i][j] = u[i][j] + ALPHA * (u[i-1][j] + u[i+1][j] + u[i][j-1] + u[i][j+1] - 4 u[i][j])
 * ALPHA = 0.2 (stable for ALPHA <= 0.25).
 *
 * STENCIL() is the single definition of the update used by every version, so
 * all versions perform bit-identical arithmetic (we compile with -std=c11,
 * which implies -ffp-contract=off: no FMA contraction differences).
 */
#ifndef HEAT_COMMON_H
#define HEAT_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define ALPHA 0.2

#define T_TOP    100.0
#define T_BOTTOM   0.0
#define T_LEFT    75.0
#define T_RIGHT   50.0

static inline double initial_value(long i, long j, long N)
{
    if (i == 0)     return T_TOP;
    if (i == N - 1) return T_BOTTOM;
    if (j == 0)     return T_LEFT;
    if (j == N - 1) return T_RIGHT;
    return 0.0;
}

#define STENCIL(c, up, down, left, right) \
    ((c) + ALPHA * ((up) + (down) + (left) + (right) - 4.0 * (c)))

typedef struct { long N; int T; int K; } heat_args_t;

/* ./prog [N] [T] [K] ...   defaults N = 1024, T = 500, K = 50 */
static inline heat_args_t heat_parse(int argc, char **argv)
{
    heat_args_t a = { 1024, 500, 50 };
    if (argc > 1) a.N = atol(argv[1]);
    if (argc > 2) a.T = atoi(argv[2]);
    if (argc > 3) a.K = atoi(argv[3]);
    if (a.N < 3 || a.T < 1 || a.K < 1) { fprintf(stderr, "need N >= 3, T >= 1, K >= 1\n"); exit(1); }
    return a;
}

#endif

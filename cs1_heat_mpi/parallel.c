/*
 * parallel.c - MPI 2-D heat diffusion with halo exchange (Case Study 1).
 *
 * Usage: mpirun -np p ./parallel [N] [T] [K] [1d|2d] [blocking|nonblocking|nbtest] [dumpfile]
 *        defaults: N = 1024, T = 500, K = 50, 2d, nonblocking
 *
 * Decomposition: a p-process Cartesian topology (MPI_Cart_create).
 *   1d -> dims = p x 1   (row blocks: each rank owns N/p full rows)
 *   2d -> dims from MPI_Dims_create(p, 2)  (blocks of about N/sqrt(p) x N/sqrt(p))
 * Each rank stores its block plus a one-cell ghost ring: (nr+2) x (nc+2).
 *
 * Halo exchange every step (rows are contiguous, columns use an MPI vector type):
 *   blocking     4 x MPI_Sendrecv, then update all owned cells
 *   nonblocking  post MPI_Irecv/MPI_Isend for the 4 edges, update the INNER
 *                cells (which need no ghost values) while messages are in
 *                flight, MPI_Waitall, then update the outer frame of cells
 *   nbtest       as nonblocking, but calls MPI_Testall every few rows of the
 *                inner update to let the MPI library progress the transfers
 *
 * Every K steps: local max |change| -> MPI_Allreduce(MAX) = global convergence metric.
 * Timing: MPI_Barrier + MPI_Wtime around the time loop, max over ranks.
 * Per-phase times (compute, halo communication, allreduce) are also reported
 * as the max over ranks.
 * If dumpfile is given, the final grid is written with MPI-IO in the same
 * raw format as serial.c, so `cmp` can check bit-for-bit equality.
 */
#include <mpi.h>
#include "heat_common.h"

enum { BLOCKING, NONBLOCKING, NBTEST };

typedef struct {
    long N, r0, c0;        /* global index of first owned row / column      */
    long nr, nc;           /* owned rows / columns                          */
    long ld;               /* leading dimension = nc + 2                    */
    long ilo, ihi, jlo, jhi; /* local index range of cells to UPDATE (incl.) */
} block_t;

#define IDX(b, i, j) ((i) * (b)->ld + (j))

/* update local rows [i0,i1] x cols [j0,j1] (clamped to the update range);
 * returns the max |change| if track != 0 */
static double update_rect(const block_t *b, const double *u, double *v,
                          long i0, long i1, long j0, long j1, int track)
{
    if (i0 < b->ilo) i0 = b->ilo;
    if (i1 > b->ihi) i1 = b->ihi;
    if (j0 < b->jlo) j0 = b->jlo;
    if (j1 > b->jhi) j1 = b->jhi;
    double md = 0.0;
    const long ld = b->ld;
    for (long i = i0; i <= i1; i++) {
        if (track) {
            for (long j = j0; j <= j1; j++) {
                const long c = i * ld + j;
                v[c] = STENCIL(u[c], u[c - ld], u[c + ld], u[c - 1], u[c + 1]);
                const double d = fabs(v[c] - u[c]);
                if (d > md) md = d;
            }
        } else {
            for (long j = j0; j <= j1; j++) {
                const long c = i * ld + j;
                v[c] = STENCIL(u[c], u[c - ld], u[c + ld], u[c - 1], u[c + 1]);
            }
        }
    }
    return md;
}

static inline double dmax(double a, double b) { return a > b ? a : b; }

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    int world, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &world);

    const heat_args_t a = heat_parse(argc, argv);
    const long N = a.N;
    const int use2d = !(argc > 4 && !strcmp(argv[4], "1d"));
    int mode = NONBLOCKING;
    if (argc > 5 && !strcmp(argv[5], "blocking")) mode = BLOCKING;
    if (argc > 5 && !strcmp(argv[5], "nbtest"))   mode = NBTEST;
    const char *dump = argc > 6 ? argv[6] : NULL;
    static const char *MODE_NAME[] = { "blocking", "nonblocking", "nbtest" };

    /* ---- Cartesian topology ---- */
    int dims[2] = { 0, 0 }, periods[2] = { 0, 0 }, coords[2];
    if (use2d) MPI_Dims_create(world, 2, dims);
    else { dims[0] = world; dims[1] = 1; }
    MPI_Comm cart;
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &cart);
    MPI_Comm_rank(cart, &rank);
    MPI_Cart_coords(cart, rank, 2, coords);
    int up, down, left, right;
    MPI_Cart_shift(cart, 0, 1, &up, &down);
    MPI_Cart_shift(cart, 1, 1, &left, &right);

    /* ---- my block ---- */
    block_t b;
    b.N  = N;
    b.r0 = coords[0] * N / dims[0];  b.nr = (coords[0] + 1) * N / dims[0] - b.r0;
    b.c0 = coords[1] * N / dims[1];  b.nc = (coords[1] + 1) * N / dims[1] - b.c0;
    b.ld = b.nc + 2;
    if (b.nr < 2 || b.nc < 2) {
        if (rank == 0) fprintf(stderr, "blocks too small for N=%ld on %dx%d\n", N, dims[0], dims[1]);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    /* local i <-> global r0 + i - 1; update only global interior [1, N-2] */
    b.ilo = (b.r0 == 0) ? 2 : 1;
    b.ihi = (b.r0 + b.nr == N) ? b.nr - 1 : b.nr;
    b.jlo = (b.c0 == 0) ? 2 : 1;
    b.jhi = (b.c0 + b.nc == N) ? b.nc - 1 : b.nc;

    const size_t cells = (size_t)(b.nr + 2) * b.ld;
    double *u = malloc(cells * sizeof(double));
    double *v = malloc(cells * sizeof(double));
    if (!u || !v) { fprintf(stderr, "rank %d: out of memory\n", rank); MPI_Abort(MPI_COMM_WORLD, 1); }
    for (long i = 0; i < b.nr + 2; i++)
        for (long j = 0; j < b.ld; j++) {
            const long gi = b.r0 + i - 1, gj = b.c0 + j - 1;
            const double val = (gi >= 0 && gi < N && gj >= 0 && gj < N) ? initial_value(gi, gj, N) : 0.0;
            u[IDX(&b, i, j)] = v[IDX(&b, i, j)] = val;
        }

    MPI_Datatype rowt, colt;
    MPI_Type_contiguous((int)b.nc, MPI_DOUBLE, &rowt);
    MPI_Type_vector((int)b.nr, 1, (int)b.ld, MPI_DOUBLE, &colt);
    MPI_Type_commit(&rowt);
    MPI_Type_commit(&colt);

    /* halo values received per step by this rank */
    const long halo = (up != MPI_PROC_NULL) * b.nc + (down != MPI_PROC_NULL) * b.nc
                    + (left != MPI_PROC_NULL) * b.nr + (right != MPI_PROC_NULL) * b.nr;

    double t_comp = 0, t_comm = 0, t_red = 0, gmax = 0;
    int checks = 0;

    MPI_Barrier(cart);
    const double t0 = MPI_Wtime();
    for (int t = 1; t <= a.T; t++) {
        const int track = (t % a.K == 0);
        double md = 0.0, ts;

        if (mode == BLOCKING) {
            ts = MPI_Wtime();
            /* send my top row up, receive my bottom ghost row from below; etc. */
            MPI_Sendrecv(&u[IDX(&b, 1, 1)],      1, rowt, up,    0,
                         &u[IDX(&b, b.nr + 1, 1)], 1, rowt, down,  0, cart, MPI_STATUS_IGNORE);
            MPI_Sendrecv(&u[IDX(&b, b.nr, 1)],   1, rowt, down,  1,
                         &u[IDX(&b, 0, 1)],        1, rowt, up,    1, cart, MPI_STATUS_IGNORE);
            MPI_Sendrecv(&u[IDX(&b, 1, 1)],      1, colt, left,  2,
                         &u[IDX(&b, 1, b.nc + 1)], 1, colt, right, 2, cart, MPI_STATUS_IGNORE);
            MPI_Sendrecv(&u[IDX(&b, 1, b.nc)],   1, colt, right, 3,
                         &u[IDX(&b, 1, 0)],        1, colt, left,  3, cart, MPI_STATUS_IGNORE);
            t_comm += MPI_Wtime() - ts;

            ts = MPI_Wtime();
            md = update_rect(&b, u, v, 1, b.nr, 1, b.nc, track);
            t_comp += MPI_Wtime() - ts;
        } else {
            MPI_Request req[8];
            ts = MPI_Wtime();
            MPI_Irecv(&u[IDX(&b, 0, 1)],        1, rowt, up,    1, cart, &req[0]);
            MPI_Irecv(&u[IDX(&b, b.nr + 1, 1)], 1, rowt, down,  0, cart, &req[1]);
            MPI_Irecv(&u[IDX(&b, 1, 0)],        1, colt, left,  3, cart, &req[2]);
            MPI_Irecv(&u[IDX(&b, 1, b.nc + 1)], 1, colt, right, 2, cart, &req[3]);
            MPI_Isend(&u[IDX(&b, 1, 1)],        1, rowt, up,    0, cart, &req[4]);
            MPI_Isend(&u[IDX(&b, b.nr, 1)],     1, rowt, down,  1, cart, &req[5]);
            MPI_Isend(&u[IDX(&b, 1, 1)],        1, colt, left,  2, cart, &req[6]);
            MPI_Isend(&u[IDX(&b, 1, b.nc)],     1, colt, right, 3, cart, &req[7]);
            t_comm += MPI_Wtime() - ts;

            /* inner cells: local rows 2..nr-1, cols 2..nc-1 need no ghost values */
            ts = MPI_Wtime();
            if (mode == NONBLOCKING) {
                md = update_rect(&b, u, v, 2, b.nr - 1, 2, b.nc - 1, track);
            } else {               /* NBTEST: poke MPI every 16 rows */
                int done = 0;
                for (long i = 2; i <= b.nr - 1; i += 16) {
                    const long i1 = (i + 15 < b.nr - 1) ? i + 15 : b.nr - 1;
                    md = dmax(md, update_rect(&b, u, v, i, i1, 2, b.nc - 1, track));
                    if (!done) {
                        const double tt = MPI_Wtime();
                        MPI_Testall(8, req, &done, MPI_STATUSES_IGNORE);
                        t_comm += MPI_Wtime() - tt;
                        ts += MPI_Wtime() - tt;     /* keep Testall out of compute time */
                    }
                }
            }
            t_comp += MPI_Wtime() - ts;

            ts = MPI_Wtime();
            MPI_Waitall(8, req, MPI_STATUSES_IGNORE);
            t_comm += MPI_Wtime() - ts;

            /* outer frame: first/last owned row, first/last owned column */
            ts = MPI_Wtime();
            md = dmax(md, update_rect(&b, u, v, 1, 1, 1, b.nc, track));
            if (b.nr > 1) md = dmax(md, update_rect(&b, u, v, b.nr, b.nr, 1, b.nc, track));
            md = dmax(md, update_rect(&b, u, v, 2, b.nr - 1, 1, 1, track));
            if (b.nc > 1) md = dmax(md, update_rect(&b, u, v, 2, b.nr - 1, b.nc, b.nc, track));
            t_comp += MPI_Wtime() - ts;
        }

        if (track) {
            ts = MPI_Wtime();
            MPI_Allreduce(&md, &gmax, 1, MPI_DOUBLE, MPI_MAX, cart);
            t_red += MPI_Wtime() - ts;
            checks++;
        }
        double *tmp = u; u = v; v = tmp;
    }
    MPI_Barrier(cart);
    const double t1 = MPI_Wtime();

    /* ---- results ---- */
    double local_sum = 0.0, sum = 0.0;
    for (long i = 1; i <= b.nr; i++)
        for (long j = 1; j <= b.nc; j++) local_sum += u[IDX(&b, i, j)];
    MPI_Reduce(&local_sum, &sum, 1, MPI_DOUBLE, MPI_SUM, 0, cart);

    double mine[4] = { t1 - t0, t_comp, t_comm, t_red }, mx[4];
    MPI_Reduce(mine, mx, 4, MPI_DOUBLE, MPI_MAX, 0, cart);
    long halo_max, cells_max, my_cells = b.nr * b.nc;
    MPI_Reduce(&halo, &halo_max, 1, MPI_LONG, MPI_MAX, 0, cart);
    MPI_Reduce(&my_cells, &cells_max, 1, MPI_LONG, MPI_MAX, 0, cart);

    if (rank == 0) {
        printf("N=%ld T=%d K=%d  p=%d  dims=%dx%d  comm=%s\n", N, a.T, a.K, world, dims[0], dims[1], MODE_NAME[mode]);
        printf("per rank   : up to %ld cells, %ld halo values received per step\n", cells_max, halo_max);
        printf("checksum   : %.10f  (sum of all cells; summation order differs from serial)\n", sum);
        printf("max change : %.6e at the last check (%d checks)\n", gmax, checks);
        printf("time       : %.4f s  (%.1f M cell-updates/s)\n", mx[0],
               (double)(N - 2) * (N - 2) * a.T / mx[0] / 1e6);
        printf("phases     : compute %.4f s, halo comm %.4f s, allreduce %.4f s  (max over ranks)\n",
               mx[1], mx[2], mx[3]);
        printf("RESULT version=parallel p=%d dims=%dx%d comm=%s N=%ld T=%d K=%d checksum=%.10f maxdiff=%.6e "
               "time=%.6f comp=%.6f comm_t=%.6f reduce=%.6f cells=%ld halo=%ld\n",
               world, dims[0], dims[1], MODE_NAME[mode], N, a.T, a.K, sum, gmax,
               mx[0], mx[1], mx[2], mx[3], cells_max, halo_max);
    }

    if (dump) {      /* every rank writes its block into the global row-major file */
        MPI_Datatype filet, memt;
        int gsz[2] = { (int)N, (int)N }, lsz[2] = { (int)b.nr, (int)b.nc }, st[2] = { (int)b.r0, (int)b.c0 };
        MPI_Type_create_subarray(2, gsz, lsz, st, MPI_ORDER_C, MPI_DOUBLE, &filet);
        int msz[2] = { (int)b.nr + 2, (int)b.ld }, mst[2] = { 1, 1 };
        MPI_Type_create_subarray(2, msz, lsz, mst, MPI_ORDER_C, MPI_DOUBLE, &memt);
        MPI_Type_commit(&filet);
        MPI_Type_commit(&memt);
        MPI_File fh;
        MPI_File_open(cart, dump, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &fh);
        MPI_File_set_size(fh, 0);
        MPI_File_set_view(fh, 0, MPI_DOUBLE, filet, "native", MPI_INFO_NULL);
        MPI_File_write_all(fh, u, 1, memt, MPI_STATUS_IGNORE);
        MPI_File_close(&fh);
        MPI_Type_free(&filet);
        MPI_Type_free(&memt);
    }

    MPI_Type_free(&rowt);
    MPI_Type_free(&colt);
    free(u); free(v);
    MPI_Comm_free(&cart);
    MPI_Finalize();
    return 0;
}
